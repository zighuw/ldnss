#include "core/LiveMeter.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static int failures = 0;

static void check(bool cond, const char* msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        ++failures;
    }
}

// Generate interleaved float samples for a sine wave.
static std::vector<float> generateSine(int sr, int ch, double freq,
                                       double peak, int nframes)
{
    std::vector<float> samples(nframes * ch);
    const double pi = std::acos(-1.0);
    for (int i = 0; i < nframes; ++i) {
        double t = static_cast<double>(i) / sr;
        float val = static_cast<float>(peak * std::sin(2.0 * pi * freq * t));
        for (int c = 0; c < ch; ++c)
            samples[i * ch + c] = val;
    }
    return samples;
}

int main()
{
    const double pi = std::acos(-1.0);

    // Test 1: Mono 1kHz sine @ -20dBFS
    {
        const int sr = 48000;
        const int ch = 1;
        const int nframes = sr * 4; // 4 seconds — enough for 3s short-term window
        const double peak = 0.1;    // -20 dBFS

        auto samples = generateSine(sr, ch, 1000.0, peak, nframes);

        LiveMeter meter(sr, ch);

        // Feed in blocks of ~10ms (480 frames)
        LiveMeterResult last;
        const int blockSize = 480;
        for (int pos = 0; pos < nframes; pos += blockSize) {
            int n = (pos + blockSize <= nframes) ? blockSize : (nframes - pos);
            last = meter.process(samples.data() + pos * ch, n);
        }

        // After 4 seconds, momentary should be close to -23 LUFS
        // (For a -20dBFS sine, each channel contributes -23 LUFS)
        printf("  Momentary LUFS: %.2f (expected ~ -23.0)\n", last.momentaryLUFS);
        check(last.momentaryLUFS < -15.0f, "momentary LUFS is not silent");

        // Short-term should also be around -23 LUFS after enough data
        printf("  Short-term LUFS: %.2f (expected ~ -23.0)\n", last.shortTermLUFS);
        check(last.shortTermLUFS < -15.0f, "short-term LUFS is not silent");

        // True peak should be close to -20 dBTP (peak = 0.1)
        printf("  True Peak: %.2f dBTP (expected ~ -20.0)\n", last.truePeakDB);
        check(std::abs(last.truePeakDB - (-20.0f)) < 2.0f,
              "true peak ≈ -20.0 (±2.0)");
    }

    // Test 2: Stereo sine — identical signals in L/R
    {
        const int sr = 48000;
        const int ch = 2;
        const int nframes = sr * 4;
        const double peak = 0.1;

        auto samples = generateSine(sr, ch, 1000.0, peak, nframes);

        LiveMeter meter(sr, ch);
        LiveMeterResult last;
        const int blockSize = 480;
        for (int pos = 0; pos < nframes; pos += blockSize) {
            int n = (pos + blockSize <= nframes) ? blockSize : (nframes - pos);
            last = meter.process(samples.data() + pos * ch, n);
        }

        // Stereo with identical signals: each channel contributes equally.
        // Per BS.1770-4, total = single channel + 3dB.
        // Single channel: -23 LUFS → stereo: ~ -20 LUFS
        printf("\n  Stereo Momentary LUFS: %.2f (expected ~ -20.0)\n",
               last.momentaryLUFS);
        check(std::abs(last.momentaryLUFS - (-20.0f)) < 2.0f,
              "stereo: momentary ≈ -20.0 (±2.0)");

        printf("  Stereo Short-term LUFS: %.2f (expected ~ -20.0)\n",
               last.shortTermLUFS);
        check(std::abs(last.shortTermLUFS - (-20.0f)) < 2.0f,
              "stereo: short-term ≈ -20.0 (±2.0)");

        // True peak: identical peaks in both channels, dB value stays the same
        printf("  Stereo True Peak: %.2f dBTP\n", last.truePeakDB);
    }

    // Test 3: Silence
    {
        const int sr = 48000;
        const int ch = 1;
        const int nframes = sr * 4;

        std::vector<float> silence(nframes * ch, 0.0f);

        LiveMeter meter(sr, ch);
        LiveMeterResult last;
        const int blockSize = 480;
        for (int pos = 0; pos < nframes; pos += blockSize) {
            int n = (pos + blockSize <= nframes) ? blockSize : (nframes - pos);
            last = meter.process(silence.data() + pos * ch, n);
        }

        printf("\n  Silence Momentary LUFS: %.2f\n", last.momentaryLUFS);
        printf("  Silence True Peak: %.2f dBTP\n", last.truePeakDB);
        check(last.truePeakDB <= -100.0f, "silence: true peak very low");
    }

    // Test 4: Reset behavior
    {
        const int sr = 48000;
        const int ch = 1;
        const int nframes = sr * 4;
        const double peak = 0.1;

        auto samples = generateSine(sr, ch, 1000.0, peak, nframes);

        LiveMeter meter(sr, ch);

        // Feed first second
        int block1 = sr * 1;
        meter.process(samples.data(), block1);

        // Reset
        meter.reset();

        // Feed remaining
        LiveMeterResult last;
        const int blockSize = 480;
        for (int pos = block1; pos < nframes; pos += blockSize) {
            int n = (pos + blockSize <= nframes) ? blockSize : (nframes - pos);
            last = meter.process(samples.data() + pos * ch, n);
        }

        // After reset, should still produce valid readings
        printf("\n  After reset, Momentary: %.2f LUFS\n", last.momentaryLUFS);
        printf("  After reset, Short-term: %.2f LUFS\n", last.shortTermLUFS);
        check(last.momentaryLUFS < -5.0f, "reset: momentary is valid");
    }

    // Test 5: Move semantics
    {
        LiveMeter a(44100, 2);
        LiveMeter b(std::move(a));
        // b should work fine
        float buf[2] = {0.1f, 0.1f};
        LiveMeterResult r = b.process(buf, 1);
        printf("\n  Move-constructed meter: TP = %.2f dBTP\n", r.truePeakDB);

        LiveMeter c(48000, 1);
        c = std::move(b);
        float buf2[1] = {0.1f};
        r = c.process(buf2, 1);
        printf("  Move-assigned meter: TP = %.2f dBTP\n", r.truePeakDB);
    }

    if (failures > 0) {
        fprintf(stderr, "\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll tests passed.\n");
    return 0;
}
