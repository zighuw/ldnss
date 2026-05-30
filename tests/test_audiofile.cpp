#include "core/SndfileDecoder.h"
#include "core/AudioFormat.h"

#include <sndfile.h>

#include <cstdio>
#include <cmath>
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

int main()
{
    // Test 1: Open nonexistent file
    {
        SndfileDecoder dec;
        check(!dec.open("/nonexistent/file/that/does/not/exist.wav"),
              "open nonexistent file should return false");
        check(!dec.isOpen(), "decoder should not be open");
    }

    // Test 2: Open a programmatically generated WAV, verify format and reading
    {
        // Generate a 1-second 1kHz sine @ 48kHz mono, peak = 0.1 (-20dBFS)
        const int sr = 48000;
        const int ch = 1;
        const double dur = 1.0;
        const int nframes = static_cast<int>(sr * dur);
        const double freq = 1000.0;
        const double peak = 0.1; // -20 dBFS → -23 LUFS for sine

        std::vector<float> samples(nframes * ch);
        for (int i = 0; i < nframes; ++i) {
            double t = static_cast<double>(i) / sr;
            const double pi = std::acos(-1.0);
            samples[i] = static_cast<float>(peak * std::sin(2.0 * pi * freq * t));
        }

        // Write via libsndfile
        SF_INFO wInfo;
        wInfo.samplerate = sr;
        wInfo.channels = ch;
        wInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
        SNDFILE* wFile = sf_open("test_sine.wav", SFM_WRITE, &wInfo);
        check(wFile != nullptr, "create test WAV for writing");
        if (wFile) {
            sf_count_t written = sf_writef_float(wFile, samples.data(), nframes);
            check(written == nframes, "write all frames");
            sf_close(wFile);
        }

        // Now read it back
        SndfileDecoder dec;
        check(dec.open("test_sine.wav"), "open test_sine.wav");
        check(dec.isOpen(), "decoder.isOpen() after open");
        check(dec.format().sampleRate == sr, "sample rate");
        check(dec.format().channels == ch, "channels");
        check(dec.format().totalFrames == nframes, "total frames");

        // Read frames
        std::vector<float> readBuf(4096 * ch);
        int64_t totalRead = 0;
        while (true) {
            int64_t n = dec.readFrames(readBuf.data(), 4096);
            if (n <= 0) break;
            totalRead += n;
        }
        check(totalRead == nframes, "read all frames");

        // Seek back and read again
        int64_t pos = dec.seekToFrame(0);
        check(pos == 0, "seek to 0");
        totalRead = 0;
        while (true) {
            int64_t n = dec.readFrames(readBuf.data(), 4096);
            if (n <= 0) break;
            totalRead += n;
        }
        check(totalRead == nframes, "read all frames after seek");

        // Clean up
        std::remove("test_sine.wav");
    }

    // Test 3: Stereo WAV
    {
        const int sr = 44100;
        const int ch = 2;
        const int nframes = 1024;

        std::vector<float> samples(nframes * ch, 0.0f);
        for (int i = 0; i < nframes; ++i) {
            samples[i * ch]     = 0.1f; // left
            samples[i * ch + 1] = 0.2f; // right
        }

        SF_INFO wInfo;
        wInfo.samplerate = sr;
        wInfo.channels = ch;
        wInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
        SNDFILE* wFile = sf_open("test_stereo.wav", SFM_WRITE, &wInfo);
        check(wFile != nullptr, "create stereo WAV");
        if (wFile) {
            sf_writef_float(wFile, samples.data(), nframes);
            sf_close(wFile);
        }

        SndfileDecoder dec;
        check(dec.open("test_stereo.wav"), "open stereo WAV");
        check(dec.format().sampleRate == sr, "stereo: sample rate");
        check(dec.format().channels == ch, "stereo: channels");
        check(dec.format().totalFrames == nframes, "stereo: total frames");

        std::remove("test_stereo.wav");
    }

    if (failures > 0) {
        fprintf(stderr, "\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("All tests passed.\n");
    return 0;
}
