#pragma once

#include "AudioFormat.h"

#include <cstdint>
#include <string>

class AudioFile {
public:
    virtual ~AudioFile() = default;

    virtual bool open(const std::string& path) = 0;

    // Read up to numFrames of interleaved float samples.
    // buffer must be sized for numFrames * channels floats.
    // Returns actual frames read (0 = EOF, negative = error).
    virtual int64_t readFrames(float* buffer, int64_t numFrames) = 0;

    // Seek to absolute frame position. Returns actual position.
    virtual int64_t seekToFrame(int64_t frame) = 0;

    virtual const AudioFormat& format() const = 0;
};
