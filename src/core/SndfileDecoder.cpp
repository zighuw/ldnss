#include "SndfileDecoder.h"

#include <sndfile.h>

SndfileDecoder::SndfileDecoder() : info_(new SF_INFO) {}

SndfileDecoder::~SndfileDecoder()
{
    close();
    delete info_;
}

SndfileDecoder::SndfileDecoder(SndfileDecoder&& other) noexcept
    : file_(other.file_)
    , info_(other.info_)
    , format_(other.format_)
{
    other.file_ = nullptr;
    other.info_ = nullptr;
}

SndfileDecoder& SndfileDecoder::operator=(SndfileDecoder&& other) noexcept
{
    if (this != &other) {
        close();
        delete info_;
        file_ = other.file_;
        info_ = other.info_;
        format_ = other.format_;
        other.file_ = nullptr;
        other.info_ = nullptr;
    }
    return *this;
}

void SndfileDecoder::close()
{
    if (file_) {
        sf_close(file_);
        file_ = nullptr;
    }
}

bool SndfileDecoder::open(const std::string& path)
{
    close();
    // Zero out SF_INFO to get clean detection
    *info_ = SF_INFO{};
    file_ = sf_open(path.c_str(), SFM_READ, info_);
    if (!file_) {
        lastError_ = sf_strerror(nullptr);
        return false;
    }

    format_.sampleRate = info_->samplerate;
    format_.channels = info_->channels;
    format_.totalFrames = info_->frames;
    return true;
}

int64_t SndfileDecoder::readFrames(float* buffer, int64_t numFrames)
{
    if (!file_)
        return -1;
    sf_count_t read = sf_readf_float(file_, buffer, static_cast<sf_count_t>(numFrames));
    return static_cast<int64_t>(read);
}

int64_t SndfileDecoder::seekToFrame(int64_t frame)
{
    if (!file_)
        return -1;
    sf_count_t pos = sf_seek(file_, static_cast<sf_count_t>(frame), SEEK_SET);
    return static_cast<int64_t>(pos);
}

const AudioFormat& SndfileDecoder::format() const
{
    return format_;
}

std::string SndfileDecoder::lastError() const
{
    return lastError_;
}

bool SndfileDecoder::isOpen() const
{
    return file_ != nullptr;
}
