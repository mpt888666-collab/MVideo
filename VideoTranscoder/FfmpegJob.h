//
// Created by mpt on 2026/11/7.
//

#ifndef VIDEOTRANSCODER_FFMPEGJOB_H
#define VIDEOTRANSCODER_FFMPEGJOB_H
#include <optional>
#include <string>

#include "Types.h"

class FfmpegJob {
public:
    explicit FfmpegJob(unsigned long long videoId) : _videoId(videoId) {}

    void Run();

private:
    unsigned long long _videoId{0};
    std::string _objectKey;
    std::optional<SourceInfo> _sourceInfo;
};


#endif //VIDEOTRANSCODER_FFMPEGJOB_H