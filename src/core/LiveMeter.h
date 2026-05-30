#pragma once

#include <cstdint>
#include <memory>

struct LiveMeterResult {
    float truePeakDB = -120.0f;
    float shortTermLUFS = -120.0f;
    float momentaryLUFS = -120.0f;
};

// Real-time per-block loudness metering.
// Wraps libebur128 with EBUR128_MODE_S for simultaneous Momentary (400ms)
// and Short-term (3s) measurement. True peak is measured separately via the
// high-precision TruePeakMeter (see TruePeakMeter.h).
//
// PIMPL: the header does not expose libebur128 types, keeping core/ free of
// Qt and PortAudio dependencies.
class LiveMeter {
public:
    LiveMeter(int sampleRate, int channels);
    ~LiveMeter();

    // Move-only (unique_ptr to Impl).
    LiveMeter(const LiveMeter&) = delete;
    LiveMeter& operator=(const LiveMeter&) = delete;
    LiveMeter(LiveMeter&&) noexcept;
    LiveMeter& operator=(LiveMeter&&) noexcept;

    // Process a block of interleaved float samples.
    // Real-time safe: no allocation, no lock, no throw.
    LiveMeterResult process(const float* buffer, int numFrames);

    // Reset internal state (e.g., after seek).
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
