//
// Created by mpt on 2026/11/7.
//

#ifndef VIDEOTRANSCODER_LADDER_H
#define VIDEOTRANSCODER_LADDER_H
#include "Types.h"
#include <vector>

class Ladder {
public:
    static std::vector<Rendition> Select(int width, int height, double fps);

    static unsigned int BitrateFor(int w, int h, double fps, int tier);
};


#endif //VIDEOTRANSCODER_LADDER_H