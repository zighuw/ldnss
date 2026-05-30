#pragma once

#include "AudioFile.h"

#include <sndfile.h>

class SndfileDecoder : public AudioFile {
public:
    SndfileDecoder();
    ~SndfileDecoder() override;

    SndfileDecoder(const SndfileDecoder&) = delete;
    SndfileDecoder& operator=(const SndfileDecoder&) = delete;
    SndfileDecoder(SndfileDecoder&&) noexcept;
    SndfileDecoder& operator=(SndfileDecoder&&) noexcept;

    bool open(const std::string& path) override;
    int64_t readFrames(float* buffer, int64_t numFrames) override;
    int64_t seekToFrame(int64_t frame) override;
    const AudioFormat& format() const override;

    bool isOpen() const;
    std::string lastError() const;

private:
    void close();

    SNDFILE* file_ = nullptr;
    SF_INFO* info_ = nullptr;
    AudioFormat format_;
    std::string lastError_;
};
