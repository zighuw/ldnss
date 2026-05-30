#include "TruePeakMeter.h"

#include <algorithm>
#include <cmath>

namespace {

// Bessel I0(x) — used by the Kaiser-window generator.
double besselI0(double x)
{
    double sum = 1.0;
    double term = 1.0;
    double x2h = (x / 2.0);
    double x2h2 = x2h * x2h;
    for (int k = 1; k <= 50; ++k) {
        term *= x2h2 / (static_cast<double>(k) * k);
        sum += term;
        if (term < 1e-15 * sum)
            break;
    }
    return sum;
}

} // anonymous namespace

TruePeakMeter::TruePeakMeter(int channels, int sampleRate)
    : channels_(channels)
    , sampleRate_(sampleRate)
    , delayPos_(0)
    , maxSquared_(0.0)
{
    designFilter();

    delayLine_.resize(channels_);
    for (int ch = 0; ch < channels_; ++ch)
        delayLine_[ch].resize(tapsPerPhase_, 0.0);
}

void TruePeakMeter::designFilter()
{
    const int N = R_ * tapsPerPhase_;   // 128
    const double D = (N - 1) / 2.0;     // 63.5 — linear-phase delay
    const double beta = 8.0;            // Kaiser beta: ~80 dB stopband
    const double pi = 3.14159265358979323846;

    // 1. Kaiser window
    double i0beta = besselI0(beta);
    std::vector<double> window(N);
    for (int n = 0; n < N; ++n) {
        double x = 2.0 * n / (N - 1) - 1.0;      // map [0, N-1] -> [-1, 1]
        window[n] = besselI0(beta * std::sqrt(1.0 - x * x)) / i0beta;
    }

    // 2. Ideal lowpass:  h_ideal[n] = sin(pi*(n-D)/R) / (pi*(n-D)/R)
    std::vector<double> h(N);
    for (int n = 0; n < N; ++n) {
        double x = pi * (n - D) / R_;
        h[n] = (std::abs(x) < 1e-12) ? 1.0 : std::sin(x) / x;
        h[n] *= window[n];
    }

    // 3. Global DC-gain correction: force sum(h[n]) = R exactly.
    double dcSum = 0.0;
    for (int n = 0; n < N; ++n)
        dcSum += h[n];
    double scale = R_ / dcSum;
    for (int n = 0; n < N; ++n)
        h[n] *= scale;

    // 4. Polyphase decomposition: subfilter p gets taps {h[p], h[p+R], ...}
    for (int p = 0; p < R_; ++p) {
        double subSum = 0.0;
        for (int k = 0; k < tapsPerPhase_; ++k) {
            coeffs_[p][k] = h[p + k * R_];
            subSum += coeffs_[p][k];
        }
        // 5. Per-subfilter normalisation — guarantees unity gain for DC at
        //    every output phase.
        double invSum = 1.0 / subSum;
        for (int k = 0; k < tapsPerPhase_; ++k)
            coeffs_[p][k] *= invSum;
    }
}

void TruePeakMeter::process(const double* interleaved, int64_t numFrames)
{
    for (int64_t frame = 0; frame < numFrames; ++frame) {
        // Insert current samples into circular delay line
        const double* src = interleaved + frame * channels_;
        for (int ch = 0; ch < channels_; ++ch)
            delayLine_[ch][delayPos_] = src[ch];

        // Compute 4x oversampled outputs (4 phases per input frame)
        for (int p = 0; p < R_; ++p) {
            for (int ch = 0; ch < channels_; ++ch) {
                double acc = 0.0;
                const double* buf = delayLine_[ch].data();
                const double* cf = coeffs_[p];
                for (int k = 0; k < tapsPerPhase_; ++k) {
                    int idx = (delayPos_ - k + tapsPerPhase_) % tapsPerPhase_;
                    acc += cf[k] * buf[idx];
                }
                double sq = acc * acc;
                if (sq > maxSquared_)
                    maxSquared_ = sq;
            }
        }

        delayPos_ = (delayPos_ + 1) % tapsPerPhase_;
    }
}

double TruePeakMeter::maxTruePeakDB() const
{
    if (maxSquared_ <= 0.0)
        return -120.0;
    double db = 20.0 * std::log10(std::sqrt(maxSquared_));
    if (db < -120.0)
        return -120.0;
    return db;
}

void TruePeakMeter::resetMax()
{
    maxSquared_ = 0.0;
}
