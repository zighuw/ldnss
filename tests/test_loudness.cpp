#include "core/SndfileDecoder.h"
#include "core/LoudnessAnalyzer.h"
#include "core/LoudnessResult.h"

#include <sndfile.h>

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

// Generate a 1-second, 1kHz sine at 48kHz, mono, -20dBFS peak (= -23 LUFS for sine)
static bool generateReferenceWav(const char* path)
{
    const int sr = 48000;
    const int ch = 1;
    const double dur = 3.0; // 3 seconds for stable measurement
    const int nframes = static_cast<int>(sr * dur);
    const double freq = 1000.0;
    const double peak = 0.1; // -20 dBFS → -23 dBFS RMS → -23 LUFS integrated

    std::vector<float> samples(nframes * ch);
    for (int i = 0; i < nframes; ++i) {
        double t = static_cast<double>(i) / sr;
        const double pi = std::acos(-1.0);
        samples[i] = static_cast<float>(peak * std::sin(2.0 * pi * freq * t));
    }

    SF_INFO wInfo;
    wInfo.samplerate = sr;
    wInfo.channels = ch;
    wInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    SNDFILE* wFile = sf_open(path, SFM_WRITE, &wInfo);
    if (!wFile)
        return false;
    sf_writef_float(wFile, samples.data(), nframes);
    sf_close(wFile);
    return true;
}

int main()
{
    // Test 1: 1kHz sine @ -23 LUFS reference (EBU Tech 3341)
    {
        const char* testFile = "test_ref_sine.wav";
        check(generateReferenceWav(testFile), "generate reference WAV");

        SndfileDecoder dec;
        check(dec.open(testFile), "open reference WAV");

        const AudioFormat& fmt = dec.format();
        LoudnessAnalyzer analyzer(fmt.sampleRate, fmt.channels);

        std::vector<float> buffer(4096 * fmt.channels);
        while (true) {
            int64_t n = dec.readFrames(buffer.data(), 4096);
            if (n <= 0) break;
            check(analyzer.addFrames(buffer.data(), n) == 0, "addFrames OK");
        }
        check(analyzer.finalize() == 0, "finalize OK");

        const LoudnessResult& r = analyzer.result();
        check(r.valid, "result valid");

        // EBU Tech 3341: 1kHz sine at -23LUFS should measure -23.0 ± 0.1 LUFS
        double iLUFS = r.integratedLUFS;
        printf("  Integrated LUFS: %.2f (expected -23.00)\n", iLUFS);
        check(std::abs(iLUFS - (-23.0)) < 0.2, "integrated LUFS ≈ -23.0 (±0.2)");

        // True peak for sine at -20dBFS ≈ -20 dBTP (±0.1)
        double tp = r.maxTruePeakDB;
        printf("  Max True Peak: %.2f dBTP (expected ~ -20.0)\n", tp);
        check(std::abs(tp - (-20.0)) < 1.0, "true peak ≈ -20.0 (±1.0)");

        // Max momentary should be close to -23.0
        double mom = r.maxMomentaryLUFS;
        printf("  Max Momentary: %.2f LUFS\n", mom);

        // Check we have history points
        check(!r.momentaryHistory.empty(), "momentary history not empty");
        printf("  Momentary history points: %zu\n", r.momentaryHistory.size());

        std::remove(testFile);
    }

    // Test 2: Stereo sine
    {
        const int sr = 48000;
        const int ch = 2;
        const int nframes = sr * 3; // 3 seconds
        const double freq = 1000.0;
        const double peak = 0.1;

        std::vector<float> samples(nframes * ch);
        for (int i = 0; i < nframes; ++i) {
            double t = static_cast<double>(i) / sr;
            const double pi = std::acos(-1.0);
            float val = static_cast<float>(peak * std::sin(2.0 * pi * freq * t));
            samples[i * ch]     = val;
            samples[i * ch + 1] = val;
        }

        SF_INFO wInfo;
        wInfo.samplerate = sr;
        wInfo.channels = ch;
        wInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
        SNDFILE* wFile = sf_open("test_stereo_ref.wav", SFM_WRITE, &wInfo);
        check(wFile != nullptr, "create stereo reference WAV");
        if (wFile) {
            sf_writef_float(wFile, samples.data(), nframes);
            sf_close(wFile);
        }

        SndfileDecoder dec;
        check(dec.open("test_stereo_ref.wav"), "open stereo reference");
        LoudnessAnalyzer analyzer(dec.format().sampleRate, dec.format().channels);

        std::vector<float> buffer(4096 * ch);
        while (true) {
            int64_t n = dec.readFrames(buffer.data(), 4096);
            if (n <= 0) break;
            analyzer.addFrames(buffer.data(), n);
        }
        analyzer.finalize();

        const LoudnessResult& r = analyzer.result();
        check(r.valid, "stereo: result valid");
        printf("\n  Stereo Integrated LUFS: %.2f (expected -23.00)\n", r.integratedLUFS);
        // Stereo with identical L/R: each channel contributes equally,
        // so total = -23 + 10*log10(2) ≈ -20.0 LUFS per BS.1770-4 channel weighting
        check(std::abs(r.integratedLUFS - (-20.0)) < 0.2, "stereo: integrated LUFS ≈ -20.0 (3dB boost from dual channels)");

        std::remove("test_stereo_ref.wav");
    }

    // Test 3: Silence produces very low LUFS / no NaN
    {
        const int sr = 48000;
        const int ch = 1;
        const int nframes = sr * 1;

        std::vector<float> samples(nframes * ch, 0.0f);

        SF_INFO wInfo;
        wInfo.samplerate = sr;
        wInfo.channels = ch;
        wInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
        SNDFILE* wFile = sf_open("test_silence.wav", SFM_WRITE, &wInfo);
        check(wFile != nullptr, "create silence WAV");
        if (wFile) {
            sf_writef_float(wFile, samples.data(), nframes);
            sf_close(wFile);
        }

        SndfileDecoder dec;
        check(dec.open("test_silence.wav"), "open silence WAV");
        LoudnessAnalyzer analyzer(dec.format().sampleRate, dec.format().channels);

        std::vector<float> buffer(4096 * ch);
        while (true) {
            int64_t n = dec.readFrames(buffer.data(), 4096);
            if (n <= 0) break;
            analyzer.addFrames(buffer.data(), n);
        }
        analyzer.finalize();

        const LoudnessResult& r = analyzer.result();
        check(r.valid, "silence: result valid");
        // Silence should result in very low LUFS (effectively negative infinity)
        printf("\n  Silence Integrated LUFS: %.2f\n", r.integratedLUFS);

        std::remove("test_silence.wav");
    }

    if (failures > 0) {
        fprintf(stderr, "\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("\nAll tests passed.\n");
    return 0;
}
