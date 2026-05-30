#include "LoudnessAnalyzer.h"

#include <ebur128.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "TruePeakMeter.h"

// ---------------------------------------------------------------------------
// LoudnessAnalyzer
// ---------------------------------------------------------------------------

static double toDB(double linear)
{
    if (linear <= 0.0)
        return -120.0;
    double db = 20.0 * std::log10(linear);
    if (db < -120.0)
        return -120.0;
    return db;
}

LoudnessAnalyzer::LoudnessAnalyzer(int sampleRate, int channels)
    : sampleRate_(sampleRate)
    , channels_(channels)
{
    int mode = EBUR128_MODE_I | EBUR128_MODE_LRA | EBUR128_MODE_TRUE_PEAK;
    state_ = ebur128_init(static_cast<unsigned int>(channels),
                          static_cast<unsigned long>(sampleRate),
                          mode);

    // Custom high-precision true-peak meter (see class docs above).
    tpMeter_.reset(new TruePeakMeter(channels, sampleRate));
}

LoudnessAnalyzer::~LoudnessAnalyzer()
{
    if (state_)
        ebur128_destroy(&state_);
}

LoudnessAnalyzer::LoudnessAnalyzer(LoudnessAnalyzer&& other) noexcept
    : state_(other.state_)
    , tpMeter_(std::move(other.tpMeter_))
    , result_(std::move(other.result_))
    , doubleBuf_(std::move(other.doubleBuf_))
    , sampleRate_(other.sampleRate_)
    , channels_(other.channels_)
    , framesProcessed_(other.framesProcessed_)
{
    other.state_ = nullptr;
}

LoudnessAnalyzer& LoudnessAnalyzer::operator=(LoudnessAnalyzer&& other) noexcept
{
    if (this != &other) {
        if (state_)
            ebur128_destroy(&state_);
        state_ = other.state_;
        tpMeter_ = std::move(other.tpMeter_);
        result_ = std::move(other.result_);
        doubleBuf_ = std::move(other.doubleBuf_);
        sampleRate_ = other.sampleRate_;
        channels_ = other.channels_;
        framesProcessed_ = other.framesProcessed_;
        other.state_ = nullptr;
    }
    return *this;
}

int LoudnessAnalyzer::addFrames(const float* buffer, int64_t numFrames)
{
    if (!state_ || numFrames <= 0)
        return -1;

    // Convert float → double for maximum true-peak oversampling precision.
    // ebur128_add_frames_double feeds double-precision samples into the 4×
    // polyphase FIR interpolator, reducing accumulated rounding noise.
    size_t n = static_cast<size_t>(numFrames) * static_cast<size_t>(channels_);
    doubleBuf_.assign(buffer, buffer + n);
    int rc = ebur128_add_frames_double(state_, doubleBuf_.data(),
                                        static_cast<size_t>(numFrames));
    if (rc != EBUR128_SUCCESS)
        return rc;

    // Feed through custom true-peak meter (double precision throughout).
    tpMeter_->process(doubleBuf_.data(), numFrames);

    framesProcessed_ += numFrames;

    // Sample momentary and short-term loudness periodically (~ every 100ms)
    // to build history and track max values.
    int sampleInterval = sampleRate_ / 10; // 100ms
    if (sampleInterval < 1)
        sampleInterval = 1;

    int64_t totalBefore = framesProcessed_ - numFrames;
    int64_t nextSample = ((totalBefore + sampleInterval) / sampleInterval) * sampleInterval;

    while (nextSample <= framesProcessed_) {
        double seconds = static_cast<double>(nextSample) / sampleRate_;

        double mom = -HUGE_VAL;
        if (ebur128_loudness_momentary(state_, &mom) == EBUR128_SUCCESS && mom > -HUGE_VAL / 2.0) {
            result_.momentaryHistory.push_back({seconds, mom});
            if (mom > result_.maxMomentaryLUFS)
                result_.maxMomentaryLUFS = mom;
        }

        double st = -HUGE_VAL;
        if (ebur128_loudness_shortterm(state_, &st) == EBUR128_SUCCESS && st > -HUGE_VAL / 2.0) {
            result_.shortTermHistory.push_back({seconds, st});
            if (st > result_.maxShortTermLUFS)
                result_.maxShortTermLUFS = st;
        }

        nextSample += sampleInterval;
    }

    return 0;
}

int LoudnessAnalyzer::addFramesDouble(const double* buffer, int64_t numFrames)
{
    if (!state_ || numFrames <= 0)
        return -1;

    int rc = ebur128_add_frames_double(state_, buffer,
                                        static_cast<size_t>(numFrames));
    if (rc != EBUR128_SUCCESS)
        return rc;

    // Feed through custom true-peak meter (double precision throughout).
    tpMeter_->process(buffer, numFrames);

    framesProcessed_ += numFrames;

    int sampleInterval = sampleRate_ / 10;
    if (sampleInterval < 1)
        sampleInterval = 1;

    int64_t totalBefore = framesProcessed_ - numFrames;
    int64_t nextSample = ((totalBefore + sampleInterval) / sampleInterval) * sampleInterval;

    while (nextSample <= framesProcessed_) {
        double seconds = static_cast<double>(nextSample) / sampleRate_;

        double mom = -HUGE_VAL;
        if (ebur128_loudness_momentary(state_, &mom) == EBUR128_SUCCESS && mom > -HUGE_VAL / 2.0) {
            result_.momentaryHistory.push_back({seconds, mom});
            if (mom > result_.maxMomentaryLUFS)
                result_.maxMomentaryLUFS = mom;
        }

        double st = -HUGE_VAL;
        if (ebur128_loudness_shortterm(state_, &st) == EBUR128_SUCCESS && st > -HUGE_VAL / 2.0) {
            result_.shortTermHistory.push_back({seconds, st});
            if (st > result_.maxShortTermLUFS)
                result_.maxShortTermLUFS = st;
        }

        nextSample += sampleInterval;
    }

    return 0;
}

int LoudnessAnalyzer::finalize()
{
    if (!state_)
        return -1;

    // Integrated LUFS
    double integrated = -HUGE_VAL;
    ebur128_loudness_global(state_, &integrated);
    if (std::isfinite(integrated))
        result_.integratedLUFS = integrated;

    // Loudness Range (LRA)
    double lra = 0.0;
    if (ebur128_loudness_range(state_, &lra) == EBUR128_SUCCESS)
        result_.loudnessRangeLU = lra;

    // Max True Peak — use our custom high-precision meter instead of
    // libebur128's float-precision 49-tap FIR.
    result_.maxTruePeakDB = tpMeter_->maxTruePeakDB();

    result_.valid = true;
    return 0;
}

const LoudnessResult& LoudnessAnalyzer::result() const
{
    return result_;
}

int64_t LoudnessAnalyzer::framesProcessed() const
{
    return framesProcessed_;
}
