# Audio Loudness Analyzer Desktop Application — Architecture Plan

## Context

Build a desktop application for professional audio loudness analysis. The application performs both **offline file analysis** (Integrated LUFS, Max True Peak, Max Momentary LUFS, Average Dynamics/LRA) and **real-time metering during playback** (True Peak, Short-term LUFS, Momentary LUFS). Target format support: WAV/FLAC/MP3 up to 96kHz/32-bit float.

Greenfield C++/Qt6 project on Windows (MSYS2 UCRT64 environment, GCC 15.2 with C++17 default).

---

## Technology Stack

| Layer | Choice | Rationale |
|-------|--------|-----------|
| Language | C++17 | GCC 15.2 default; zero-cost abstractions for perf-critical audio |
| GUI | Qt 6.10 Widgets + Charts | Mature, native, MSYS2 pkg; Charts for time-series graphs |
| Audio Decoding | libsndfile 1.2.2 (+ mpg123 transitive) | libsndfile covers WAV/FLAC/OGG/Vorbis/Opus; mpg123 for MP3 |
| Loudness Analysis | libebur128 1.2.6 | Industry standard ITU-R BS.1770-4, C library |
| Audio Playback | PortAudio 1.19.7 | Cross-platform, WASAPI backend on Windows, callback-level access |
| Build System | CMake 4.3 | Standard, available in MSYS2 |

---

## Layered Architecture

Three static libraries + one executable. The key design principle: `core` has **zero Qt or PortAudio dependency** — it is a pure C++ DSP library that can be tested headlessly.

```
┌──────────────────────────────────────────────────────────┐
│                    Qt6 GUI (ldnss.exe)                    │
│  MainWindow, AnalysisPanel, LiveMeterPanel, Waveform,    │
│  MeterBar, TransportControls, FileBrowser                │
│  Depends on: core + playback + Qt6::Widgets/Charts       │
├──────────────────────────────────────────────────────────┤
│              Playback Engine (ldnss_playback)             │
│  AudioPlayer (PortAudio stream + callback)               │
│  Depends on: core + PortAudio + Qt6::Core (signals)      │
├──────────────────────────────────────────────────────────┤
│                Core Library (ldnss_core)                  │
│  AudioFile (abstract), SndfileDecoder, LoudnessAnalyzer, │
│  LiveMeter, LiveMeterRingBuffer, LoudnessResult          │
│  Depends on: libsndfile, libebur128 (C libs only)        │
└──────────────────────────────────────────────────────────┘
```

---

## Data Flow

### 1. Offline Analysis

```
User selects file → AudioFileFactory::create(path)
  → LoudnessAnalyzer::analyze(AudioFile&) [on worker QThread]
    loop: decoder.readFrames(buffer, 4096) → addFrames(buffer, n)
    finalize() → LoudnessResult { integratedLUFS, maxTruePeakDB,
      maxMomentaryLUFS, maxShortTermLUFS, loudnessRangeLU,
      momentaryHistory[] (for charts) }
  → signal analysisComplete(result) [back on main thread]
  → AnalysisPanel + LoudnessChartWidget update
```

### 2. Real-time Playback + Metering

```
User presses Play → AudioPlayer::play()
  → PortAudio stream started [real-time audio thread]
    PA callback fires (e.g. 256 frames @ 48kHz):
      1. decoder.readFrames(buf, 256)  → interleaved float[]
      2. LiveMeter::process(buf, 256)  → LiveMeterResult { truePeakDB, stLUFS, mLUFS }
      3. ringBuffer.write(result)       ← lock-free SPSC
      4. PortAudio output ← buf         → DAC
  → Qt QTimer @ 30Hz [main thread]:
      ringBuffer.readLatest(&result) → MeterBarWidgets update
```

### 3. Thread Architecture

```
 ┌─── Main Thread (Qt Event Loop) ───┐
 │  GUI updates, 30Hz QTimer polls   │
 │  ring buffer for meter data       │
 └──┬──────────────┬─────────────────┘
    │ queued       │ atomic<PlayerCommand>
    │ signals      │ (Play/Pause/Stop/Seek)
    v              v
 ┌── Worker Thread ──┐  ┌── Audio Thread (PortAudio CB) ──┐
 │ LoudnessAnalyzer  │  │ MUST NOT: alloc, lock, throw    │
 │ (offline, QThread)│  │ decoder.read → LiveMeter → PA   │
 └───────────────────┘  │ → ringBuffer.write (lock-free)  │
                         └────────────────────────────────┘
```

---

## Project Directory Structure

```
ldnss/
├── CMakeLists.txt
├── cmake/
│   └── FindLibEbur128.cmake          # libebur128 has no .pc file
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── core/                         # === Pure C++ / C libs ONLY ===
│   │   ├── AudioFile.h               # Abstract interface
│   │   ├── AudioFormat.h             # sampleRate, channels, totalFrames
│   │   ├── SndfileDecoder.h/.cpp     # WAV/FLAC/OGG via libsndfile
│   │   ├── LoudnessAnalyzer.h/.cpp   # libebur128 wrapper (offline)
│   │   ├── LoudnessResult.h          # Value object (all loudness metrics)
│   │   ├── LiveMeter.h/.cpp          # Per-block real-time metering (PIMPL)
│   │   └── RingBuffer.h             # Template, lock-free SPSC
│   ├── playback/                     # === PortAudio + Qt::Core ===
│   │   ├── AudioPlayer.h/.cpp        # PortAudio stream manager
│   │   └── PlayerState.h             # Enum + status struct
│   └── ui/                           # === Qt6 Widgets + Charts ===
│       ├── MainWindow.h/.cpp         # Dock-based main window
│       ├── AnalysisPanel.h/.cpp      # Offline analysis tab
│       ├── LiveMeterPanel.h/.cpp     # Real-time meters tab
│       ├── MeterBarWidget.h/.cpp     # Custom-painted bar (color zones)
│       ├── LoudnessChartWidget.h/.cpp # Qt Charts time-series
│       ├── WaveformWidget.h/.cpp     # Time-domain waveform + seek cursor
│       ├── TransportControls.h/.cpp  # Play/Pause/Stop/Seek
│       ├── FileBrowserWidget.h/.cpp  # Open + drag-drop + recent files
│       └── WorkerController.h/.cpp   # QThread pool for offline analysis
├── resources/
│   ├── app.qrc
│   └── icons/
├── tests/
│   ├── CMakeLists.txt
│   ├── test_audiofile.cpp
│   ├── test_loudness.cpp
│   └── test_livemeter.cpp
└── .clang-format
```

---

## Key Class Designs

### AudioFile (abstract decoder)

```cpp
// src/core/AudioFile.h — zero Qt dependency
struct AudioFormat {
    int sampleRate;
    int channels;
    int64_t totalFrames;
};

class AudioFile {
public:
    virtual ~AudioFile() = default;
    virtual bool open(const std::string& path) = 0;
    virtual int64_t readFrames(float* buffer, int64_t numFrames) = 0;
    virtual int64_t seekToFrame(int64_t frame) = 0;
    virtual const AudioFormat& format() const = 0;
};
```

Design notes:
- All samples normalized to `[-1.0f, +1.0f]` regardless of source bit depth
- `readFrames` returns actual frames read (0 = EOF), buffer must be `numFrames * channels` floats
- Factory: static `create(path)` uses file extension + libsndfile format detection

### LoudnessResult (value object)

```cpp
// src/core/LoudnessResult.h
struct MomentaryPoint {
    double seconds;
    double lufs;
};

struct LoudnessResult {
    double integratedLUFS = 0.0;
    double maxTruePeakDB = -120.0;
    double maxMomentaryLUFS = -120.0;
    double maxShortTermLUFS = -120.0;
    double loudnessRangeLU = 0.0;         // LRA (Average Dynamics)
    std::vector<MomentaryPoint> momentaryHistory;  // for chart
    std::vector<MomentaryPoint> shortTermHistory;  // for chart
    bool valid = false;
};
```

### LoudnessAnalyzer (offline — wraps libebur128)

```cpp
// src/core/LoudnessAnalyzer.h
class LoudnessAnalyzer {
public:
    LoudnessAnalyzer(int sampleRate, int channels);
    ~LoudnessAnalyzer();

    // Block by block (streaming)
    int addFrames(const float* buffer, int64_t numFrames);
    int finalize();

    const LoudnessResult& result() const;
    int64_t framesProcessed() const;
};
```

BS.1770-4 modes: `EBUR128_MODE_I | EBUR128_MODE_LRA | EBUR128_MODE_TRUE_PEAK | EBUR128_MODE_SAMPLE_PEAK`.

### LiveMeter (real-time — PIMPL to hide libebur128 from Qt)

```cpp
// src/core/LiveMeter.h
struct LiveMeterResult {
    float truePeakDB = -120.0f;
    float shortTermLUFS = -120.0f;
    float momentaryLUFS = -120.0f;
};

class LiveMeter {
public:
    LiveMeter(int sampleRate, int channels);
    ~LiveMeter();
    // Move-only (PIMPL)
    LiveMeter(LiveMeter&&) noexcept;
    LiveMeter& operator=(LiveMeter&&) noexcept;

    // Real-time safe after construction
    LiveMeterResult process(const float* buffer, int numFrames);
    void reset();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

Uses `EBUR128_MODE_M | EBUR128_MODE_S | EBUR128_MODE_TRUE_PEAK` for Momentary (400ms) and Short-term (3s) live measurement.

### RingBuffer (lock-free SPSC)

```cpp
// src/core/RingBuffer.h
template<typename T, size_t Capacity>  // Capacity must be power of 2
class RingBuffer {
    std::array<T, Capacity> buffer_;
    alignas(64) std::atomic<size_t> writeIdx_{0};
    alignas(64) std::atomic<size_t> readIdx_{0};
public:
    bool write(const T& item);      // Producer (audio thread), returns false if full
    bool readLatest(T& out);        // Consumer (Qt main thread), skip-to-latest
};
```

### AudioPlayer (PortAudio + Qt signals)

```cpp
// src/playback/AudioPlayer.h
class AudioPlayer : public QObject {
    Q_OBJECT
public:
    void load(const std::string& path);
    void play();
    void pause();
    void stop();
    void seek(int64_t frame);

    using MeterBuffer = core::RingBuffer<core::LiveMeterResult, 64>;
    const MeterBuffer& meterBuffer() const;

signals:
    void stateChanged(PlayerState state);
    void positionChanged(int64_t frame, float seconds);
    void errorOccurred(const QString& msg);
};
```

### WorkerController (Qt threading)

```cpp
// src/ui/WorkerController.h
class WorkerController : public QObject {
    Q_OBJECT
public:
    void startAnalysis(const QString& filePath);  // non-blocking
    void cancelAnalysis();
signals:
    void analysisProgress(int percent);
    void analysisComplete(const LoudnessResult& result);
    void analysisError(const QString& msg);
};
```

---

## Build System (CMake)

### Dependency installation (one-time)

```bash
pacman -S mingw-w64-ucrt-x86_64-{qt6-base,qt6-charts,qt6-multimedia,libsndfile,portaudio,libebur128,fftw}
```

### Top-level CMakeLists.txt key points

- `find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts)`
- `pkg_check_modules(SNDFILE REQUIRED sndfile)`
- `pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)`
- Custom `FindLibEbur128.cmake` (searches `/usr/include`, `/mingw64/include`)
- Three static libs: `ldnss_core` → `ldnss_playback` → `ldnss` executable
- `ldnss_core` links: `sndfile`, `LibEbur128` (no Qt)
- `ldnss_playback` links: `ldnss_core` + `PortAudio` + `Qt6::Core`
- `ldnss` (exe) links: both libs + `Qt6::Widgets Qt6::Charts`

---

## Implementation Phases

### Phase 1: Foundation — Core Library + CLI
- CMakeLists.txt with all deps
- `AudioFile` interface + `SndfileDecoder` (WAV/FLAC/OGG/MP3)
- `LoudnessAnalyzer` wrapping libebur128 (offline mode)
- `LoudnessResult` value object
- CLI harness: `ldnss-cli --analyze song.wav` → prints all metrics to stdout
- Unit tests with reference signals (EBU Tech 3341, 1kHz @ -23LUFS)

### Phase 2: Playback Engine
- `LiveMeter` with PIMPL (real-time block processing)
- `RingBuffer` lock-free SPSC template
- `AudioPlayer` with PortAudio callback (WASAPI on Windows)
- Real-time safety: no alloc/lock/throw in callback
- Headless playback test: `ldnss-cli --play song.wav` prints live meters

### Phase 3: Qt6 GUI — Offline Analysis
- `MainWindow` (dock-based layout)
- `FileBrowserWidget` (open dialog, drag-drop)
- `AnalysisPanel` with result labels
- `WorkerController` for threaded analysis
- `LoudnessChartWidget` (Qt Charts time-series of Momentary/Short-term history)

### Phase 4: Qt6 GUI — Real-Time Metering
- `MeterBarWidget` (custom-painted, EBU target zones: green/yellow/red)
- `LiveMeterPanel` (True Peak, Short-term LUFS, Momentary LUFS bars)
- `TransportControls` (play/pause/stop/seek slider)
- `WaveformWidget` (time-domain rendering + click-to-seek)

### Phase 5: Polish
- 96kHz/32-bit float verification
- Drag-and-drop file open
- Export results as CSV/JSON
- Recent files list (QSettings)
- Error handling: corrupt files, unsupported formats, audio device failures
- HiDPI verification (Qt6 handles this natively)

---

## Verification Plan

1. **Build**: `cmake -B build && cmake --build build` — zero errors
2. **Unit tests**: `ctest --output-on-failure` — all pass
3. **Reference signal test**: EBU Tech 3341 1kHz sine @ -23 LUFS → Integrated LUFS = -23.0 ± 0.1
4. **Format matrix**: WAV (16/24/32-bit int, 32-bit float), FLAC, MP3 — all decode correctly
5. **High sample rate**: 96kHz/32-bit float WAV → correct analysis, no crashes
6. **Real-time test**: Play a file → meters update at 30Hz, no audio glitches or underruns
7. **Thread safety**: Analyze a large file while playing another → both work, no deadlocks
