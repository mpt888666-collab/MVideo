//
// Created by mpt on 2026/11/7.
//

#include "FfmpegRunner.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/stdio.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>
#include <boost/asio/steady_timer.hpp>

#include "ConfigMgr.h"
#include "ThreadPools.h"
#include "ThreadPools.h"

namespace {
    namespace bp = boost::process::v2;

    constexpr auto kProbeTimeout = std::chrono::seconds(15);
    constexpr std::size_t kErrorTail = 800;

    struct RunResult {
        int exit_code{-1}; //0是正常退出
        bool timed_out{false};
        std::string out;
        std::string err;
        std::string spawn_error;
    };

    std::string ConfigValue(const char* key, const char* fallback) {
        const std::string value = ConfigMgr::Inst()["FFmpeg"][key];
        return value.empty() ? std::string(fallback) : value;
    }

    std::string Tail(const std::string& text, std::size_t limit) {
        if (text.size() <= limit) {
            return text;
        }
        return text.substr(text.size() - limit);
    }

    RunResult Run(const std::string& exe, const std::vector<std::string>& args,
                  std::chrono::seconds timeout) {
        RunResult result;
        try {
            boost::asio::io_context ctx;
            boost::asio::readable_pipe out{ctx};
            boost::asio::readable_pipe err{ctx};

            bp::process proc(ctx, exe, args, bp::process_stdio{nullptr, out, err});

            std::atomic<bool> done{false};
            std::thread watchdog([&] {
                const auto deadline = std::chrono::steady_clock::now() + timeout;
                while (!done.load(std::memory_order_acquire)) {
                    if (std::chrono::steady_clock::now() >= deadline) {
                        result.timed_out = true;
                        try {
                            proc.terminate();
                        } catch (...) {
                        }
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });

            std::thread err_reader([&] {
                boost::system::error_code ec;
                boost::asio::read(err, boost::asio::dynamic_buffer(result.err), ec);
            });
            boost::system::error_code ec;
            boost::asio::read(out, boost::asio::dynamic_buffer(result.out), ec);
            err_reader.join();

            done.store(true, std::memory_order_release);
            watchdog.join();
            result.exit_code = proc.wait();
        } catch (const std::exception& e) {
            result.spawn_error = e.what();
        }
        return result;
    }

    const nlohmann::json* FindMember(const nlohmann::json& node, const char* key) {
        if (!node.is_object()) {
            return nullptr;
        }
        const auto it = node.find(key);
        if (it == node.end() || it->is_null()) {
            return nullptr;
        }
        return &(*it);
    }

    std::string StrValue(const nlohmann::json& node, const char* key) {
        const auto* value = FindMember(node, key);
        if (!value) {
            return {};
        }
        if (value->is_string()) {
            return value->get<std::string>();
        }
        if (value->is_number_integer()) {
            return std::to_string(value->get<long long>());
        }
        if (value->is_number_unsigned()) {
            return std::to_string(value->get<unsigned long long>());
        }
        if (value->is_number_float()) {
            return std::to_string(value->get<double>());
        }
        return {};
    }

    int IntValue(const nlohmann::json& node, const char* key, int fallback = 0) {
        const auto* value = FindMember(node, key);
        if (!value) {
            return fallback;
        }
        if (value->is_number_integer()) {
            return value->get<int>();
        }
        if (value->is_number_unsigned()) {
            return static_cast<int>(value->get<unsigned long long>());
        }
        if (value->is_string()) {
            try {
                return std::stoi(value->get<std::string>());
            } catch (...) {
                return fallback;
            }
        }
        return fallback;
    }

    unsigned long long U64Value(const nlohmann::json& node, const char* key) {
        const auto* value = FindMember(node, key);
        if (!value) {
            return 0;
        }
        if (value->is_number_unsigned()) {
            return value->get<unsigned long long>();
        }
        if (value->is_number_integer()) {
            const auto number = value->get<long long>();
            return number > 0 ? static_cast<unsigned long long>(number) : 0;
        }
        if (value->is_string()) {
            try {
                return std::stoull(value->get<std::string>());
            } catch (...) {
                return 0;
            }
        }
        return 0;
    }

    double DoubleValue(const nlohmann::json& node, const char* key, double fallback = 0.0) {
        const auto* value = FindMember(node, key);
        if (!value) {
            return fallback;
        }
        if (value->is_number()) {
            return value->get<double>();
        }
        if (value->is_string()) {
            try {
                return std::stod(value->get<std::string>());
            } catch (...) {
                return fallback;
            }
        }
        return fallback;
    }

    double ParseFraction(const std::string& text) {
        if (text.empty()) {
            return 0.0;
        }
        const auto slash = text.find('/');
        if (slash == std::string::npos) {
            try {
                return std::stod(text);
            } catch (...) {
                return 0.0;
            }
        }
        try {
            const double numerator = std::stod(text.substr(0, slash));
            const double denominator = std::stod(text.substr(slash + 1));
            if (denominator <= 0.0) {
                return 0.0;
            }
            return numerator / denominator;
        } catch (...) {
            return 0.0;
        }
    }
}

ProbeResult FfmpegRunner::Probe(const std::string& inputFile) {
    ProbeResult result;

    std::error_code fs_ec;
    const std::filesystem::path input(inputFile);
    if (!std::filesystem::exists(input, fs_ec) ||
        !std::filesystem::is_regular_file(input, fs_ec)) {
        result.error = "input file not found: " + inputFile;
        return result;
    }

    const std::string ffprobe = ConfigValue("ProbePath", "ffprobe.exe");
    const std::vector<std::string> args = {
            "-v", "error", "-print_format", "json", "-show_format", "-show_streams", inputFile,
    };

    const RunResult run = Run(ffprobe, args, kProbeTimeout);
    if (!run.spawn_error.empty()) {
        result.error = "failed to start ffprobe (" + ffprobe + "): " + run.spawn_error;
        return result;
    }
    if (run.timed_out) {
        result.error = "ffprobe timed out";
        return result;
    }
    if (run.exit_code != 0) {
        result.error = "ffprobe exit " + std::to_string(run.exit_code) + ": " +
                       Tail(run.err, kErrorTail);
        return result;
    }

    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(run.out);
    } catch (const std::exception& e) {
        result.error = std::string("ffprobe output is not valid json: ") + e.what();
        return result;
    }

    const auto streams_it = doc.find("streams");
    if (streams_it == doc.end() || !streams_it->is_array()) {
        result.error = "ffprobe output has no streams";
        return result;
    }

    const nlohmann::json* video = nullptr;
    const nlohmann::json* audio = nullptr;
    for (const auto& stream : *streams_it) {
        const std::string type = StrValue(stream, "codec_type");
        if (type == "video" && video == nullptr) {
            // 跳过封面图这类 0x0 的“视频流”
            if (IntValue(stream, "width") > 0 && IntValue(stream, "height") > 0) {
                video = &stream;
            }
        } else if (type == "audio" && audio == nullptr) {
            audio = &stream;
        }
    }

    if (video == nullptr) {
        result.error = "no video stream found";
        return result;
    }

    result.width = IntValue(*video, "width");
    result.height = IntValue(*video, "height");
    result.video_codec = StrValue(*video, "codec_name");
    result.pix_fmt = StrValue(*video, "pix_fmt");
    result.bitrate = U64Value(*video, "bit_rate");

    result.fps = ParseFraction(StrValue(*video, "avg_frame_rate"));
    if (result.fps <= 0.0) {
        result.fps = ParseFraction(StrValue(*video, "r_frame_rate"));
    }

    // 时长优先取视频流，没有就取容器
    double duration_sec = DoubleValue(*video, "duration", 0.0);
    if (duration_sec <= 0.0) {
        const auto format_it = doc.find("format");
        if (format_it != doc.end()) {
            duration_sec = DoubleValue(*format_it, "duration", 0.0);
            if (result.bitrate == 0) {
                result.bitrate = U64Value(*format_it, "bit_rate");
            }
        }
    }
    if (duration_sec > 0.0) {
        result.duration_ms = static_cast<unsigned int>(std::llround(duration_sec * 1000.0));
    }

    // 手机竖拍：rotation 在 side_data_list 里，老版本放在 tags.rotate
    if (const auto* side_data = FindMember(*video, "side_data_list");
        side_data && side_data->is_array()) {
        for (const auto& item : *side_data) {
            //rotation 旋转标记
            if (FindMember(item, "rotation")) {
                result.rotation = IntValue(item, "rotation");
                break;
            }
        }
    }
    if (result.rotation == 0) {
        if (const auto* tags = FindMember(*video, "tags"); tags && tags->is_object()) {
            result.rotation = IntValue(*tags, "rotate");
        }
    }
    result.rotation = ((result.rotation % 360) + 360) % 360;

    if (audio != nullptr) {
        result.has_audio = true;
        result.audio_codec = StrValue(*audio, "codec_name");
        result.sample_rate = static_cast<unsigned int>(IntValue(*audio, "sample_rate"));
        result.channels = IntValue(*audio, "channels");
    }

    result.ok = true;
    return result;
}

namespace {
    constexpr int kSegmentSeconds = 4;
    constexpr auto kHlsMinTimeout = std::chrono::seconds(120);
    constexpr auto kHlsMaxTimeout = std::chrono::seconds(7200);

    std::string JoinPath(const std::string& base, const std::string& name) {
        if (base.empty()) {
            return name;
        }
        if (base.back() == '/' || base.back() == '\\') {
            return base + name;
        }
        return base + "/" + name;
    }

    bool IsRegularFile(const std::string& path) {
        std::error_code ec;
        return std::filesystem::is_regular_file(std::filesystem::path(path), ec);
    }

    unsigned long long FileSize(const std::string& path) {
        std::error_code ec;
        const auto size = std::filesystem::file_size(std::filesystem::path(path), ec);
        return ec ? 0ULL : static_cast<unsigned long long>(size);
    }

    // 每个 job 分到的 x264 线程数 = 核数 / 并发 job 数，避免多档并发把 CPU 打满
    int ThreadsPerJob() {
        const unsigned cores = std::max(1u, std::thread::hardware_concurrency());
        std::size_t jobs = 1;
        if (auto pool = EncodeWorkPool::GetInstance()) {
            jobs = std::max<std::size_t>(1, pool->MaxConcurrent());
        }
        const unsigned per_job = cores / static_cast<unsigned>(jobs);
        return static_cast<int>(std::max(1u, per_job));
    }

    bool CreateDir(const std::string& dir, std::string& error) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(dir), ec);
        if (ec) {
            error = "create directory failed: " + dir + " (" + ec.message() + ")";
            return false;
        }
        return true;
    }
}  // namespace

HlsOutput FfmpegRunner::TranscodeToHls(const std::string& local,
                                       const std::vector<Rendition>& ladder,
                                       const std::string& outDir, double fps, bool hasAudio,
                                       unsigned int durationMs) {
    HlsOutput result;

    if (!IsRegularFile(local)) {
        result.error = "input file not found: " + local;
        return result;
    }
    if (ladder.empty()) {
        result.error = "ladder is empty";
        return result;
    }
    if (outDir.empty()) {
        result.error = "outDir is empty";
        return result;
    }
    if (outDir.size() < 4) {
        result.error = "outDir looks unsafe: " + outDir;
        return result;
    }

    std::error_code cleanup_ec;
    std::filesystem::remove_all(std::filesystem::path(outDir), cleanup_ec);
    if (cleanup_ec) {
        result.error = "clear output directory failed: " + outDir + " (" + cleanup_ec.message() + ")";
        return result;
    }

    const std::string ffmpeg = ConfigValue("Path", "ffmpeg.exe");
    const double safe_fps = (fps > 0.0 && fps < 1000.0) ? fps : 30.0;
    const int gop = std::max(1, static_cast<int>(std::llround(safe_fps * kSegmentSeconds)));
    const int n = static_cast<int>(ladder.size());

    for (const auto& rendition : ladder) {
        std::string error;
        if (!CreateDir(JoinPath(outDir, rendition.quality), error)) {
            result.error = error;
            return result;
        }
    }

    std::vector<std::string> args = {
            "-nostdin", "-y", "-hide_banner", "-loglevel", "error",
            "-i", local,
    };

    if (n == 1) {
        const std::string graph = "[0:v]scale=" + std::to_string(ladder[0].width) + ":" +
                                  std::to_string(ladder[0].height) + "[v0o]";
        args.insert(args.end(), {"-filter_complex", graph});
        args.insert(args.end(), {"-map", "[v0o]"});
        if (hasAudio) {
            args.insert(args.end(), {"-map", "0:a:0"});
        }
    } else {
        std::string graph = "[0:v]split=" + std::to_string(n);
        for (int i = 0; i < n; ++i) {
            graph += "[v" + std::to_string(i) + "]";
        }
        for (int i = 0; i < n; ++i) {
            graph += ";[v" + std::to_string(i) + "]scale=" + std::to_string(ladder[i].width) + ":" +
                     std::to_string(ladder[i].height) + "[v" + std::to_string(i) + "o]";
        }
        args.insert(args.end(), {"-filter_complex", graph});

        for (int i = 0; i < n; ++i) {
            args.insert(args.end(), {"-map", "[v" + std::to_string(i) + "o]"});
            if (hasAudio) {
                args.insert(args.end(), {"-map", "0:a:0"});
            }
        }
    }

    args.insert(args.end(), {"-c:v", "libx264", "-preset", "veryfast",
                             "-profile:v", "main", "-pix_fmt", "yuv420p"});
    args.insert(args.end(), {"-threads", std::to_string(ThreadsPerJob())});

    if (hasAudio) {
        args.insert(args.end(), {"-c:a", "aac", "-b:a", "128k", "-ac", "2", "-ar", "48000"});
    } else {
        args.push_back("-an");
    }

    for (int i = 0; i < n; ++i) {
        const std::string idx = ":" + std::to_string(i);
        const unsigned int bitrate = ladder[i].bitrate;
        args.insert(args.end(), {"-b:v" + idx, std::to_string(bitrate)});
        args.insert(args.end(), {"-maxrate:v" + idx, std::to_string(bitrate * 107 / 100)});
        args.insert(args.end(), {"-bufsize:v" + idx, std::to_string(bitrate * 2)});
    }

    // 关键帧对齐：所有档位必须落在同一时间点，否则切清晰度会花屏
    args.insert(args.end(), {"-g", std::to_string(gop),
                             "-keyint_min", std::to_string(gop),
                             "-sc_threshold", "0",
                             "-force_key_frames",
                             "expr:gte(t,n_forced*" + std::to_string(kSegmentSeconds) + ")",
                             "-sn", "-dn"});

    args.insert(args.end(), {"-f", "hls",
                             "-hls_time", std::to_string(kSegmentSeconds),
                             "-hls_playlist_type", "vod",
                             "-hls_flags", "independent_segments",
                             "-master_pl_name", "master.m3u8"});

    std::string vsm;
    for (int i = 0; i < n; ++i) {
        if (i > 0) {
            vsm += " ";
        }
        vsm += "v:" + std::to_string(i);
        if (hasAudio) {
            vsm += ",a:" + std::to_string(i);
        }
        vsm += ",name:" + ladder[i].quality;
    }
    args.insert(args.end(), {"-var_stream_map", vsm});

    args.push_back("-hls_segment_filename");
    args.push_back(JoinPath(outDir, "%v/seg_%05d.ts"));
    args.push_back(JoinPath(outDir, "%v/index.m3u8"));

    std::chrono::seconds timeout = kHlsMaxTimeout;
    if (durationMs > 0) {
        const long long guess = static_cast<long long>(durationMs / 1000) * 5;
        timeout = std::chrono::seconds(std::max(kHlsMinTimeout.count(), guess));
    }

    const RunResult run = Run(ffmpeg, args, timeout);
    if (!run.spawn_error.empty()) {
        result.error = "failed to start ffmpeg (" + ffmpeg + "): " + run.spawn_error;
        return result;
    }
    if (run.timed_out) {
        result.error = "ffmpeg timed out after " + std::to_string(timeout.count()) + "s";
        return result;
    }
    if (run.exit_code != 0) {
        result.error = "ffmpeg exit " + std::to_string(run.exit_code) + ": " +
                       Tail(run.err, kErrorTail);
        return result;
    }

    // exit code 0 不代表产物齐全，逐个校验
    const std::string master = JoinPath(outDir, "master.m3u8");
    if (!IsRegularFile(master) || FileSize(master) == 0) {
        result.error = "master playlist missing: " + master + " " + Tail(run.err, 200);
        return result;
    }

    const int expected_segments = durationMs > 0
            ? std::max(1, static_cast<int>(std::ceil(durationMs / 1000.0 / kSegmentSeconds)))
            : 0;

    std::vector<std::string> segments;
    std::vector<std::string> playlists;

    for (const auto& rendition : ladder) {
        const std::string dir = JoinPath(outDir, rendition.quality);
        const std::string index = JoinPath(dir, "index.m3u8");
        if (!IsRegularFile(index) || FileSize(index) == 0) {
            result.error = "variant playlist missing: " + index;
            return result;
        }
        playlists.push_back(index);

        int count = 0;
        std::error_code ec;
        for (const auto& entry :
             std::filesystem::directory_iterator(std::filesystem::path(dir), ec)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            if (entry.path().extension() == ".ts") {
                segments.push_back(entry.path().string());
                ++count;
            }
        }
        if (ec) {
            result.error = "scan output failed: " + dir + " (" + ec.message() + ")";
            return result;
        }
        if (count == 0) {
            result.error = "no segment produced for " + rendition.quality;
            return result;
        }
        if (expected_segments > 0 && std::abs(count - expected_segments) > 1) {
            result.error = "segment count mismatch for " + rendition.quality + ": expected ~" +
                           std::to_string(expected_segments) + ", got " + std::to_string(count);
            return result;
        }
    }

    unsigned long long total_bytes = 0;
    for (const auto& segment : segments) {
        total_bytes += FileSize(segment);
    }
    if (total_bytes == 0) {
        result.error = "all segments are empty";
        return result;
    }

    // directory_iterator 顺序不保证，排序后日志和重试对比才稳定
    std::sort(segments.begin(), segments.end());

    // 上传顺序：先分片，后播放列表，避免客户端拿到指向不存在分片的 m3u8
    result.files = std::move(segments);
    result.files.insert(result.files.end(), playlists.begin(), playlists.end());
    result.files.push_back(master);
    result.master_playlist = master;
    result.ok = true;
    return result;
}
namespace {
    constexpr auto kCoverTimeout = std::chrono::seconds(30);
}

bool FfmpegRunner::ExtractCover(const std::string& inputFile, const std::string& outputFile,
                                double seekSeconds, std::string* error) {
    const auto fail_with = [error](const std::string& message) {
        if (error) {
            *error = message;
        }
        return false;
    };

    if (!IsRegularFile(inputFile)) {
        return fail_with("input file not found: " + inputFile);
    }
    if (outputFile.empty()) {
        return fail_with("cover output path is empty");
    }

    const std::filesystem::path out_path(outputFile);
    if (out_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(out_path.parent_path(), ec);
        if (ec) {
            return fail_with("create cover directory failed: " + ec.message());
        }
    }

    const std::string ffmpeg = ConfigValue("Path", "ffmpeg.exe");
    const double seek = seekSeconds > 0.0 ? seekSeconds : 1.0;

    // -ss 放 -i 前面是快速定位：不解码前面那段，封面不需要帧级精度
    const std::vector<std::string> args = {
            "-nostdin", "-y", "-hide_banner", "-loglevel", "error",
            "-ss", std::to_string(seek),
            "-i", inputFile,
            "-frames:v", "1",
            "-an",
            "-vf", "scale=480:-2",
            "-q:v", "3",
            outputFile,
    };

    const RunResult run = Run(ffmpeg, args, kCoverTimeout);
    if (!run.spawn_error.empty()) {
        return fail_with("failed to start ffmpeg (" + ffmpeg + "): " + run.spawn_error);
    }
    if (run.timed_out) {
        return fail_with("ffmpeg cover timed out");
    }
    if (run.exit_code != 0) {
        return fail_with("ffmpeg cover exit " + std::to_string(run.exit_code) + ": " +
                         Tail(run.err, kErrorTail));
    }
    if (!IsRegularFile(outputFile) || FileSize(outputFile) == 0) {
        return fail_with("cover file was not produced: " + outputFile);
    }
    return true;
}