//
// Created by mpt on 2026/11/7.
//

#include "FfmpegJob.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <utility>

#include "ConfigMgr.h"
#include "FfmpegRunner.h"
#include "Ladder.h"
#include "MinioClient.h"
#include "MySqlMgr.h"

namespace {
    std::string RelativeTo(const std::string& file, const std::string& root) {
        const auto rel = std::filesystem::path(file).lexically_relative(std::filesystem::path(root));
        if (rel.empty()) {
            return std::filesystem::path(file).filename().generic_string();
        }
        return rel.generic_string();
    }

    std::string LowerExtension(const std::string& key) {
        const auto dot = key.find_last_of('.');
        if (dot == std::string::npos) {
            return {};
        }
        std::string ext = key.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext;
    }

    const char* ContentTypeFor(const std::string& key) {
        const std::string ext = LowerExtension(key);
        if (ext == ".m3u8") {
            return "application/vnd.apple.mpegurl";
        }
        if (ext == ".ts") {
            return "video/mp2t";
        }
        if (ext == ".jpg" || ext == ".jpeg") {
            return "image/jpeg";
        }
        return "application/octet-stream";
    }

    unsigned long long LocalFileSize(const std::string& path) {
        std::error_code ec;
        const auto size = std::filesystem::file_size(std::filesystem::path(path), ec);
        return ec ? 0ULL : static_cast<unsigned long long>(size);
    }
}

void FfmpegJob::Run() {
    auto db = MySqlMgr::GetInstance();
    if (!db->ClaimVideo(_videoId)) {
        std::cout << "[job " << _videoId << "] claim failed" << std::endl;
        return;
    }

    const auto fail = [this, db](const std::string& reason) {
        std::cerr << "[job " << _videoId << "] " << reason << std::endl;
        db->FailTranscode(_videoId, reason);
    };

    auto info = db->GetSourceInfo(_videoId);
    if (!info) {
        fail("GetSourceInfo failed");
        return;
    }
    _sourceInfo = info;
    _objectKey = info->object_key;

    std::string temp_root = ConfigMgr::Inst()["FFmpeg"]["TempDir"];
    if (temp_root.empty()) {
        temp_root = "C:/mvideo-temp";
    }

    const std::string work_dir = temp_root + "/" + std::to_string(_videoId);
    const std::string local = work_dir + "/source.mp4";
    const std::string out_dir = work_dir + "/out";

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(work_dir), ec);
    if (ec) {
        fail("create work dir failed: " + ec.message());
        return;
    }

    if (!MinioClient::Instance().Download(info->object_key, local)) {
        fail("download source failed: " + info->object_key);
        return;
    }

    const ProbeResult probe = FfmpegRunner::Probe(local);
    if (!probe.ok) {
        fail("probe failed: " + probe.error);
        return;
    }

    int display_width = probe.width;
    int display_height = probe.height;
    if (probe.rotation == 90 || probe.rotation == 270) {
        std::swap(display_width, display_height);
    }

    auto ladder = Ladder::Select(display_width, display_height, probe.fps);
    if (ladder.empty()) {
        Rendition fallback;
        fallback.quality = "source";
        fallback.width = display_width & ~1;
        fallback.height = display_height & ~1;
        fallback.bitrate = Ladder::BitrateFor(fallback.width, fallback.height, probe.fps,
                                              (std::min)(display_width, display_height));
        ladder.push_back(fallback);
    }

    const HlsOutput output = FfmpegRunner::TranscodeToHls(local, ladder, out_dir, probe.fps,
                                                          probe.has_audio, probe.duration_ms);
    if (!output.ok) {
        fail("transcode failed: " + output.error);
        return;
    }

    std::cout << "[job " << _videoId << "] hls ready, files = " << output.files.size()
              << ", master = " << output.master_playlist << std::endl;

    const std::string cover_local = work_dir + "/cover.jpg";
    const double cover_seek = (std::min)(1.0, probe.duration_ms / 1000.0 * 0.1);

    std::string cover_error;
    if (!FfmpegRunner::ExtractCover(local, cover_local, cover_seek, &cover_error)) {
        fail("cover failed: " + cover_error);
        return;
    }

    std::cout << "[job " << _videoId << "] cover ready: " << cover_local << std::endl;

    const std::string object_prefix = "videos/" + std::to_string(_sourceInfo->uid) + "/" +
                                      std::to_string(_videoId) + "/";

    struct UploadItem {
        std::string local;
        std::string key;
    };

    std::vector<UploadItem> uploads;
    uploads.reserve(output.files.size() + 1);

    for (const auto& file : output.files) {
        uploads.push_back({file, object_prefix + "hls/" + RelativeTo(file, out_dir)});
    }
    uploads.push_back({cover_local, object_prefix + "cover.jpg"});

    unsigned long long uploaded_bytes = 0;
    for (const auto& item : uploads) {
        const unsigned long long size = LocalFileSize(item.local);
        if (size == 0) {
            fail("local file is empty: " + item.local);
            return;
        }

        if (!MinioClient::Instance().Upload(item.local, item.key, ContentTypeFor(item.key))) {
            fail("upload failed: " + item.key);
            return;
        }

        const auto remote = MinioClient::Instance().HeadObjectSize(item.key);
        if (!remote || *remote != size) {
            fail("upload verify failed: " + item.key + " local=" + std::to_string(size) +
                 " remote=" + (remote ? std::to_string(*remote) : std::string("missing")));
            return;
        }
        uploaded_bytes += size;
    }

    std::cout << "[job " << _videoId << "] uploaded " << uploads.size() << " objects, "
              << uploaded_bytes << " bytes, prefix = " << object_prefix << std::endl;

    EncodeResult result;
    result.video_id = _videoId;
    result.duration_ms = probe.duration_ms;
    result.width = static_cast<unsigned int>(display_width);
    result.height = static_cast<unsigned int>(display_height);
    result.fps = probe.fps;
    result.cover_key = object_prefix + "cover.jpg";

    for (const auto& rendition : ladder) {
        const std::string quality_prefix = rendition.quality + "/";
        unsigned long long rendition_bytes = 0;
        for (const auto& file : output.files) {
            if (RelativeTo(file, out_dir).rfind(quality_prefix, 0) == 0) {
                rendition_bytes += LocalFileSize(file);
            }
        }

        EncodeAssetInfo asset;
        asset.asset_type = "hls";
        asset.quality = rendition.quality;
        asset.width = static_cast<unsigned int>(rendition.width);
        asset.height = static_cast<unsigned int>(rendition.height);
        asset.bitrate = rendition.bitrate;
        asset.codec = "h264";
        asset.storage_key = object_prefix + "hls/" + rendition.quality + "/index.m3u8";
        asset.file_size = rendition_bytes;
        result.assets.push_back(std::move(asset));
    }

    {
        const Rendition& top = ladder.front();
        EncodeAssetInfo asset;
        asset.asset_type = "hls";
        asset.quality = "master";
        asset.width = static_cast<unsigned int>(top.width);
        asset.height = static_cast<unsigned int>(top.height);
        asset.bitrate = top.bitrate;
        asset.codec = "h264";
        asset.storage_key = object_prefix + "hls/master.m3u8";
        asset.file_size = LocalFileSize(output.master_playlist);
        result.assets.push_back(std::move(asset));
    }

    {
        const unsigned int cover_width = 480;
        unsigned int cover_height = static_cast<unsigned int>(
                std::llround(480.0 * display_height / display_width));
        cover_height &= ~1u;
        if (cover_height < 2) {
            cover_height = 2;
        }

        EncodeAssetInfo asset;
        asset.asset_type = "cover";
        asset.quality = "master";
        asset.width = cover_width;
        asset.height = cover_height;
        asset.bitrate = 0;
        asset.codec = "mjpeg";
        asset.storage_key = object_prefix + "cover.jpg";
        asset.file_size = LocalFileSize(cover_local);
        result.assets.push_back(std::move(asset));
    }

    if (!db->CommitTranscode(result)) {
        fail("CommitTranscode failed");
        return;
    }

    std::cout << "[job " << _videoId << "] committed, assets = " << result.assets.size()
              << std::endl;


}