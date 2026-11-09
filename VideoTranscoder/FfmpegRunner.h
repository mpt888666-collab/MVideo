//
// Created by mpt on 2026/11/7.
//

#ifndef VIDEOTRANSCODER_FFMPEGRUNNER_H
#define VIDEOTRANSCODER_FFMPEGRUNNER_H

#include <string>

#include "Types.h"

class FfmpegRunner {
public:
    FfmpegRunner() = default;

    static ProbeResult Probe(const std::string& inputFile);

    // 多档 HLS 一次编完；产物落在 outDir/<quality>/ 与 outDir/master.m3u8。
    // durationMs > 0 时用时长推算看门狗超时，否则用固定上限。
    static HlsOutput TranscodeToHls(const std::string& local, const std::vector<Rendition>& ladder,
                                    const std::string& outDir, double fps, bool hasAudio,
                                    unsigned int durationMs = 0);

    // 抽一帧做封面。seekSeconds 取 1 秒或时长 10%，别用 0（常是黑场）。
    static bool ExtractCover(const std::string& inputFile, const std::string& outputFile,
                             double seekSeconds, std::string* error = nullptr);
};

#endif  // VIDEOTRANSCODER_FFMPEGRUNNER_H