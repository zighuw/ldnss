#pragma once

#include <cstdint>

struct AudioFormat {
    int sampleRate = 0;
    int channels = 0;
    int64_t totalFrames = 0;
};
