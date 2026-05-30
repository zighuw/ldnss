#include "LiveMeter.h"
#include "TruePeakMeter.h"

#include <ebur128.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

struct LiveMeter::Impl {
    ebur128_state* state = nullptr;
    std::unique_ptr<TruePeakMeter> tpMeter;
    std::vector<double> doubleBuf_;
    int sampleRate = 0;
    int channels = 0;

    Impl(int sr, int ch)
        : sampleRate(sr), channels(ch)
    {
        // True peak is now measured by our custom high-precision meter,
        // so we only need EBUR128_MODE_S for LUFS.
        int mode = EBUR128_MODE_S;
        state = ebur128_init(static_cast<unsigned int>(ch),
                             static_cast<unsigned long>(sr), mode);
        tpMeter.reset(new TruePeakMeter(ch, sr));
    }

    ~Impl()
    {
        if (state)
            ebur128_destroy(&state);
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    void reinit()
    {
        if (state)
            ebur128_destroy(&state);
        int mode = EBUR128_MODE_S;
        state = ebur128_init(static_cast<unsigned int>(channels),
                             static_cast<unsigned long>(sampleRate), mode);
        tpMeter.reset(new TruePeakMeter(channels, sampleRate));
    }
};

LiveMeter::LiveMeter(int sampleRate, int channels)
    : impl_(new Impl(sampleRate, channels))
{
}

LiveMeter::~LiveMeter() = default;

LiveMeter::LiveMeter(LiveMeter&&) noexcept = default;
LiveMeter& LiveMeter::operator=(LiveMeter&&) noexcept = default;

LiveMeterResult LiveMeter::process(const float* buffer, int numFrames)
{
    LiveMeterResult result;
    if (!impl_ || !impl_->state || numFrames <= 0)
        return result;

    ebur128_state* st = impl_->state;
    size_t nf = static_cast<size_t>(numFrames);

    // Convert float → double for both libebur128 (double-precision feed) and
    // the custom TruePeakMeter (see TruePeakMeter.h for rationale).
    size_t totalSamples = nf * static_cast<size_t>(impl_->channels);
    impl_->doubleBuf_.resize(totalSamples);
    for (size_t i = 0; i < totalSamples; ++i)
        impl_->doubleBuf_[i] = static_cast<double>(buffer[i]);

    ebur128_add_frames_double(st, impl_->doubleBuf_.data(), nf);

    // Momentary (400ms window)
    double mom = -HUGE_VAL;
    if (ebur128_loudness_momentary(st, &mom) == EBUR128_SUCCESS
        && mom > -HUGE_VAL / 2.0 && std::isfinite(mom)) {
        result.momentaryLUFS = static_cast<float>(mom);
    }

    // Short-term (3s window)
    double stLUFS = -HUGE_VAL;
    if (ebur128_loudness_shortterm(st, &stLUFS) == EBUR128_SUCCESS
        && stLUFS > -HUGE_VAL / 2.0 && std::isfinite(stLUFS)) {
        result.shortTermLUFS = static_cast<float>(stLUFS);
    }

    // True peak via custom high-precision meter
    impl_->tpMeter->process(impl_->doubleBuf_.data(),
                            static_cast<int64_t>(numFrames));
    result.truePeakDB = static_cast<float>(impl_->tpMeter->maxTruePeakDB());
    impl_->tpMeter->resetMax();

    return result;
}

void LiveMeter::reset()
{
    if (impl_)
        impl_->reinit();
}
