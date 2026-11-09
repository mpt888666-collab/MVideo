//
// Created by mpt on 2026/11/2.
//

#ifndef VIDEOTRANSCODER_CONST_H
#define VIDEOTRANSCODER_CONST_H
#include <functional>
#include <string>

struct RequestInfo {
    unsigned long long _video_id{0};

    std::string object_key;

    std::function<void()> callback;

    std::function<void()> on_finished;
};

#endif //VIDEOTRANSCODER_CONST_H