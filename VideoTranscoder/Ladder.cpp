//
// Created by mpt on 2026/11/7.
//

#include "Ladder.h"

#include <algorithm>

#include "Types.h"
#include <cmath>

unsigned int Ladder::BitrateFor(int w, int h, double fps, int tier) {
    const double bpp = tier >= 720 ? 0.10 : tier >= 480 ? 0.09 : 0.10;
    double validFps = (fps > 0 && fps < 1000) ? fps : 30.0;
    double bps = double(w) * h * validFps * bpp;
    bps = std::clamp(bps, 600e3, 8e6);
    return static_cast<unsigned int>(std::floor(bps / 1000.0)) * 1000;
}

std::vector<Rendition> Ladder::Select(int width, int height, double fps) {
    std::vector<Rendition> result;
    if (width <= 0 || height <= 0) return result;

    const int shortSide = std::min(width, height);
    const int longSide  = std::max(width, height);
    const bool portrait = height > width;

    static constexpr int kTargetShortSides[] = {1080, 720, 480, 360};

    for (int targetShort : kTargetShortSides) {
        if (targetShort > shortSide) {
            continue;
        }

        int s = targetShort;
        int l = static_cast<int>(std::llround(static_cast<double>(longSide) * s / shortSide));

        // 强制偶数（H.264/H.265 编码要求）
        s &= ~1;
        l &= ~1;

        if (s < 2 || l < 2) continue;

        Rendition r;
        r.quality = std::to_string(targetShort) + "p";
        if (portrait) {
            r.width  = s;
            r.height = l;
        } else {
            r.width  = l;
            r.height = s;
        }
        r.bitrate = BitrateFor(r.width, r.height, fps, targetShort);

        if (!result.empty() &&
            result.back().width == r.width &&
            result.back().height == r.height) {
            continue;
            }
        result.push_back(std::move(r));
    }
    return result;
}
