//
// Created by mpt on 2026/11/6.
//

#ifndef VIDEOTRANSCODER_TYPES_H
#define VIDEOTRANSCODER_TYPES_H
#include <string>
#include <vector>
// 一档清晰度的产物，对应 video_asset 一行
struct EncodeAssetInfo {
    std::string asset_type;   // original / hls / cover
    std::string quality;      // master / 720p / 360p
    unsigned int width{0};
    unsigned int height{0};
    unsigned int bitrate{0};
    std::string codec;
    std::string storage_key;
    unsigned long long file_size{0};
};

// 一次转码的完整结果，CommitTranscode 用单事务写进去
struct EncodeResult {
    unsigned long long video_id{0};
    unsigned int duration_ms{0};
    unsigned int width{0};
    unsigned int height{0};
    double fps{0.0};
    std::string cover_key;
    std::vector<EncodeAssetInfo> assets;
};

// 转码需要的源信息：MinIO 对象 key + 作者 id（产物 key 要用 uid）
struct SourceInfo {
    std::string object_key;
    unsigned long long uid{0};
};

// 视频原始元信息（ffprobe 读到的“事实”，不是目标参数）
struct ProbeResult {
    bool ok{false};
    std::string error;

    int width{0};
    int height{0};
    double fps{0.0};
    unsigned int duration_ms{0};

    std::string video_codec;        // h264 / hevc
    std::string pix_fmt;            // yuv420p / yuv444p
    unsigned long long bitrate{0};  // bps
    int rotation{0};                // 0/90/180/270，手机竖拍

    bool has_audio{false};
    std::string audio_codec;        // aac / opus
    unsigned int sample_rate{0};
    int channels{0};
};

struct Rendition {
    std::string quality;
    int width{0}, height{0};
    unsigned int bitrate{0};
};

// TranscodeToHls 的产物：校验结果 + 待上传的本地文件
struct HlsOutput {
    bool ok{false};
    std::string error;
    std::string master_playlist;
    std::vector<std::string> files;   // 先分片、后播放列表，按序上传即可
};

#endif //VIDEOTRANSCODER_TYPES_H