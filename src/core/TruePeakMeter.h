#pragma once

#include <cstdint>
#include <vector>

// Custom high-precision true-peak oversampler.
//
// Uses a double-precision 128-tap Kaiser-windowed polyphase FIR filter (4x
// oversampling, beta=8, ~80 dB stopband attenuation) to measure true peak
// with significantly lower passband ripple than libebur128's 49-tap float
// Hamming-windowed FIR.
//
// The meter keeps a running maximum of squared oversampled values.  For
// per-block readings (e.g. live metering), call maxTruePeakDB() followed by
// resetMax() after each block.
class TruePeakMeter {
public:
    TruePeakMeter(int channels, int sampleRate);

    // Process interleaved double-precision audio frames. Real-time safe:
    // no allocation, no lock, no throw.
    void process(const double* interleaved, int64_t numFrames);

    // Return the running maximum true-peak value in dBTP, clamped to
    // [-120.0, +inf). Does NOT reset the internal accumulator.
    double maxTruePeakDB() const;

    // Reset the running maximum to zero (for per-block live-meter readings).
    void resetMax();

private:
    void designFilter();

    int channels_;
    int sampleRate_;
    static constexpr int R_ = 4;
    static constexpr int tapsPerPhase_ = 32;

    double coeffs_[4][32];
    std::vector<std::vector<double>> delayLine_;
    int delayPos_;
    double maxSquared_;
};
