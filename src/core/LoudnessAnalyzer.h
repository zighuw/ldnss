#pragma once

#include "LoudnessResult.h"

#include <ebur128.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "TruePeakMeter.h"

class LoudnessAnalyzer {
public:
    LoudnessAnalyzer(int sampleRate, int channels);
    ~LoudnessAnalyzer();

    LoudnessAnalyzer(const LoudnessAnalyzer&) = delete;
    LoudnessAnalyzer& operator=(const LoudnessAnalyzer&) = delete;
    LoudnessAnalyzer(LoudnessAnalyzer&&) noexcept;
    LoudnessAnalyzer& operator=(LoudnessAnalyzer&&) noexcept;

    // Add interleaved float frames (internally converts to double for
    // maximum true-peak oversampling precision). Returns 0 on success.
    int addFrames(const float* buffer, int64_t numFrames);

    // Add interleaved double frames directly (no conversion overhead).
    // Prefer this when source data is already double precision.
    int addFramesDouble(const double* buffer, int64_t numFrames);

    // Finalize analysis. Must be called after all frames added.
    int finalize();

    const LoudnessResult& result() const;
    int64_t framesProcessed() const;

private:
    ebur128_state* state_ = nullptr;
    std::unique_ptr<TruePeakMeter> tpMeter_;    // custom high-precision TP meter
    LoudnessResult result_;
    std::vector<double> doubleBuf_; // float→double conversion buffer
    int sampleRate_;
    int channels_;
    int64_t framesProcessed_ = 0;
};
