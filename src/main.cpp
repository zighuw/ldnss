#include "core/AudioFile.h"
#include "core/AudioFormat.h"
#include "core/SndfileDecoder.h"
#include "core/LoudnessAnalyzer.h"
#include "core/LoudnessResult.h"
#include "core/LiveMeter.h"
#include "playback/AudioPlayer.h"
#include "playback/PlayerState.h"

#include <QCoreApplication>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static const char* VERSION = "1.0.0";
static const int BUFFER_FRAMES = 4096;

static void printUsage(const char* prog)
{
    printf("ldnss-cli — Audio Loudness Analyzer %s\n\n", VERSION);
    printf("Usage:\n");
    printf("  %s --analyze <file>   Run offline loudness analysis\n", prog);
    printf("  %s --play <file>       Play file with live metering\n", prog);
    printf("  %s --version           Print version\n", prog);
    printf("  %s --help              Print this help\n", prog);
}

static void printResult(const char* path, const AudioFormat& fmt,
                        const LoudnessResult& r)
{
    printf("File: %s\n", path);
    printf("Sample Rate: %d Hz  Channels: %d  Frames: %lld\n",
           fmt.sampleRate, fmt.channels, (long long)fmt.totalFrames);
    printf("---\n");
    printf("Integrated LUFS:  %+.1f\n", r.integratedLUFS);
    printf("Max True Peak:    %+.1f dBTP\n", r.maxTruePeakDB);
    printf("Max Momentary:    %+.1f LUFS\n", r.maxMomentaryLUFS);
    printf("Max Short-term:   %+.1f LUFS\n", r.maxShortTermLUFS);
    printf("Loudness Range:   %.1f LU\n", r.loudnessRangeLU);
    printf("---\n");
    printf("Analysis complete.\n");
}

static int runAnalyze(const char* path)
{
    SndfileDecoder decoder;
    if (!decoder.open(path)) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", path);
        return 1;
    }

    const AudioFormat& fmt = decoder.format();
    printf("Analyzing: %s\n", path);
    printf("  %d Hz, %d channels, %lld frames (%.1f s)\n",
           fmt.sampleRate,
           fmt.channels,
           (long long)fmt.totalFrames,
           static_cast<double>(fmt.totalFrames) / fmt.sampleRate);

    LoudnessAnalyzer analyzer(fmt.sampleRate, fmt.channels);
    std::vector<float> buffer(static_cast<size_t>(BUFFER_FRAMES * fmt.channels));

    while (true) {
        int64_t n = decoder.readFrames(buffer.data(), BUFFER_FRAMES);
        if (n <= 0)
            break;
        if (analyzer.addFrames(buffer.data(), n) != 0) {
            fprintf(stderr, "Error: Analysis failed at frame %lld\n",
                    (long long)analyzer.framesProcessed());
            return 1;
        }
        double progress = static_cast<double>(analyzer.framesProcessed())
                          / fmt.totalFrames * 100.0;
        printf("\r  Progress: %.0f%%", progress);
        fflush(stdout);
    }
    printf("\n");

    if (analyzer.finalize() != 0) {
        fprintf(stderr, "Error: Finalize failed\n");
        return 1;
    }

    printf("\n");
    printResult(path, fmt, analyzer.result());
    return 0;
}

// ---------------------------------------------------------------------------
// --play mode
// ---------------------------------------------------------------------------

static int runPlay(const char* path)
{
    // QCoreApplication is needed for QObject signal/slot + QTimer.
    int argc = 1;
    char* argv[] = {const_cast<char*>("ldnss-cli"), nullptr};
    QCoreApplication app(argc, argv);

    AudioPlayer player;
    bool finished = false;
    int exitCode = 0;

    QObject::connect(&player, &AudioPlayer::stateChanged,
                     [&](PlayerState s) {
        if (s == PlayerState::Stopped) {
            printf("\nPlayback finished.\n");
            finished = true;
            app.quit();
        } else if (s == PlayerState::Error) {
            fprintf(stderr, "\nPlayback error.\n");
            exitCode = 1;
            finished = true;
            app.quit();
        }
    });

    QObject::connect(&player, &AudioPlayer::positionChanged,
                     [&](int64_t /*frame*/, float seconds) {
        printf("\r  Position: %.1f s", seconds);
        fflush(stdout);
    });

    QObject::connect(&player, &AudioPlayer::errorOccurred,
                     [](const QString& msg) {
        fprintf(stderr, "Error: %s\n", qPrintable(msg));
    });

    if (!player.load(path)) {
        return 1;
    }

    printf("Playing: %s\n", path);

    // QTimer to poll the meter ring buffer at ~30 Hz
    QTimer meterTimer;
    QObject::connect(&meterTimer, &QTimer::timeout, [&]() {
        LiveMeterResult result;
        if (player.meterBuffer().readLatest(result)) {
            printf("\r  TP: %+.1f dBTP  |  St: %+.1f LUFS  |  Mo: %+.1f LUFS",
                   result.truePeakDB, result.shortTermLUFS, result.momentaryLUFS);
            fflush(stdout);
        }
    });
    meterTimer.start(33);

    player.play();

    app.exec();

    meterTimer.stop();
    player.stop();

    return exitCode;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printUsage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printf("ldnss-cli version %s\n", VERSION);
        return 0;
    }

    if (strcmp(argv[1], "--analyze") == 0 || strcmp(argv[1], "-a") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: --analyze requires a file path\n");
            return 1;
        }
        return runAnalyze(argv[2]);
    }

    if (strcmp(argv[1], "--play") == 0 || strcmp(argv[1], "-p") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: --play requires a file path\n");
            return 1;
        }
        return runPlay(argv[2]);
    }

    fprintf(stderr, "Error: Unknown option '%s'\n", argv[1]);
    printUsage(argv[0]);
    return 1;
}
