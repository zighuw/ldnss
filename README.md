# ldnss — Audio Loudness Analyzer

A Windows (MSYS2/MinGW) desktop application for offline and real-time audio
loudness analysis compliant with **ITU-R BS.1770-4** (EBU R128).  Provides both
a graphical interface and a command-line tool.

## Features

- **Offline analysis** — integrated LUFS, true-peak (over-sampled), momentary
  max, short-term max, and loudness range (LRA) with progress reporting
- **Real-time live metering** — true-peak, momentary LUFS, and short-term LUFS
  during playback via PortAudio
- **Waveform display** — click-to-seek on a down-sampled peak envelope
- **Time-series chart** — momentary and short-term LUFS history from the last
  analysis run (Qt Charts)
- **Drag-and-drop** — supports WAV, FLAC, Ogg, MP3, AIFF
- **Recent files** — last 10 files persisted via QSettings
- **Static linking** — libsndfile, 7 codecs, and PortAudio are compiled in
  (zero "extra" DLLs beyond Qt and the MSYS2 runtime)
- **UPX compressed** — shipped binaries are compressed ~67% with no runtime
  overhead for plugins

## Architecture

Three static libraries in a strict dependency chain:

```
┌─────────────────────────────────────────────────┐
│          Qt6 GUI  (ldnss.exe, WIN32)             │
│  Qt6::Widgets + Qt6::Charts                     │
├─────────────────────────────────────────────────┤
│       Playback  (ldnss_playback)                 │
│  PortAudio + Qt6::Core (QObject signals/slots)   │
├─────────────────────────────────────────────────┤
│          Core  (ldnss_core)                      │
│  libsndfile + libebur128 — pure C++, zero Qt     │
└─────────────────────────────────────────────────┘
```

Three runtime threads:
1. **Main thread** — Qt event loop, GUI updates, 30 Hz meter polling from a
   lock-free ring buffer
2. **Worker thread** (QThread) — offline `LoudnessAnalyzer`, communicates via
   queued signals
3. **Audio thread** (PortAudio callback) — real-time critical: no allocation,
   no lock, no throw, no Qt signals

## Dependencies

Build-time (MSYS2 `pacman`):

```
mingw-w64-ucrt-x86_64-{gcc,cmake,ninja}
mingw-w64-ucrt-x86_64-qt6-base
mingw-w64-ucrt-x86_64-qt6-charts
mingw-w64-ucrt-x86_64-libsndfile
mingw-w64-ucrt-x86_64-portaudio
mingw-w64-ucrt-x86_64-libebur128
```

`libebur128` does not ship a `.pc` file, so a custom `FindLibEbur128.cmake`
module is included in `cmake/`.

Run-time (included in `output/`):
- 6 Qt DLLs (deployed by `windeployqt`)
- 4 Qt plugin DLLs (`platforms/`, `imageformats/`, `styles/`)
- 24 MSYS2/ucrt64 runtime DLLs
- libsndfile and PortAudio are **statically linked** — no separate DLLs needed

## Build

```bash
./build.sh                 # cmake configure + build + test
```

Or manually:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Package

```bash
./package.sh               # deploy + UPX + clean
./package.sh --no-clean    # keep build/ directory
```

The two scripts split the pipeline into build and package phases:

**build.sh** (`./build.sh`):

| Step | Action |
|------|--------|
| 1 | CMake configure (Release, Ninja) |
| 2 | Build 34 targets |
| 3 | Run 3 test suites |

**package.sh** (`./package.sh`) — requires a completed build:

| Step | Action |
|------|--------|
| 1 | Prepare `output/` |
| 2 | Copy executables |
| 3 | `windeployqt` + remove unused plugins |
| 4 | Copy 24 non-Qt runtime DLLs |
| 5 | UPX `--best --lzma` on all top-level binaries |
| 6 | Clean `build/` |

Final output: **36 files, ~29 MB** in `output/`.

## Usage

### GUI

```bash
output/ldnss.exe
```

- Drag an audio file onto the window or click **Browse…**
- Playback controls: Play/Pause, Stop, seek slider, Reset Meters
- Click on the waveform to seek
- Live meters update in real time during playback
- Offline analysis results appear in the right panel with a time-series chart
- Recent files are persisted across sessions (File → Recent Files)

### CLI

```bash
output/ldnss-cli.exe --analyze <file>   # offline analysis
output/ldnss-cli.exe --play <file>      # playback with live metering
output/ldnss-cli.exe --version
output/ldnss-cli.exe --help
```

**`--analyze` output example:**

```
File:    music.wav
Sample rate: 44100 Hz
Channels:    2
Frames:      8704512
Duration:    197.38 s

[===============>] 100%

Integrated LUFS:  -14.2
Max True Peak:    +1.3 dBTP
Max Momentary:    -10.1 LUFS
Max Short-term:   -12.8 LUFS
Loudness Range:   3.7 LU
```

**`--play` output example:**

```
Playing music.wav (44100 Hz, 2 ch, 197.4 s)
[  3.2s] TP:  -1.4  St: -13.1  Mo: -11.2
[  3.5s] TP:  -1.3  St: -13.0  Mo: -11.0
```

## Project Structure

```
ldnss/
├── CMakeLists.txt              # top-level: project, C++17, AUTOMOC
├── ARCHITECTURE.md             # detailed design rationale
├── README.md
├── build.sh                    # cmake configure + build + test
├── package.sh                  # deploy + UPX + clean
├── cmake/
│   └── FindLibEbur128.cmake    # custom find-module (no .pc file)
├── src/
│   ├── CMakeLists.txt          # 4 targets: core, playback, CLI, GUI
│   ├── main.cpp                # CLI entry point
│   ├── main_gui.cpp            # GUI entry point
│   ├── core/                   # pure C++ DSP (no Qt / PortAudio)
│   │   ├── AudioFile.h         # abstract decoder interface
│   │   ├── AudioFormat.h       # sample rate / channels / frames
│   │   ├── SndfileDecoder.h/cpp
│   │   ├── LoudnessAnalyzer.h/cpp
│   │   ├── LoudnessResult.h
│   │   ├── LiveMeter.h/cpp
│   │   └── RingBuffer.h        # lock-free SPSC queue
│   ├── playback/               # PortAudio + Qt6::Core
│   │   ├── AudioPlayer.h/cpp
│   │   └── PlayerState.h
│   └── ui/                     # Qt6 Widgets + Charts
│       ├── MainWindow.h/cpp
│       ├── AnalysisPanel.h/cpp
│       ├── AnalysisWorker.h/cpp
│       ├── WorkerController.h/cpp
│       ├── LoudnessChartWidget.h/cpp
│       ├── FileBrowserWidget.h/cpp
│       ├── MeterBarWidget.h/cpp
│       ├── TransportControls.h/cpp
│       ├── WaveformWidget.h/cpp
│       └── LiveMeterPanel.h/cpp
└── tests/
    ├── CMakeLists.txt
    ├── test_audiofile.cpp      # SndfileDecoder open/read/seek
    ├── test_loudness.cpp       # LoudnessAnalyzer reference signals
    └── test_livemeter.cpp      # LiveMeter real-time blocks
```

## Tests

3 test executables run via CTest:

| Test | What it validates |
|------|-------------------|
| `test_audiofile` | File open failure, mono/stereo decode, seek, format readback |
| `test_loudness` | EBU Tech 3341 reference tone (-23 LUFS), stereo weighting (+3 dB), silence robustness |
| `test_livemeter` | Real-time block processing, stereo boost, silence floor, reset, move semantics |

Tests programmatically generate WAV files via libsndfile — no test assets needed.

## Technical Notes

- **EBU R128 compliance**: Core analysis uses `libebur128` with modes
  `EBUR128_MODE_I | LRA | TRUE_PEAK | SAMPLE_PEAK` for offline and
  `EBUR128_MODE_M | MODE_S | TRUE_PEAK` for live metering.
- **Real-time safety**: The PortAudio callback only calls `decoder->readFrames`,
  `meter->process`, and `ringBuffer->write`.  No heap allocation, no lock
  acquisition, no exception throw, no Qt signal emission.
- **Lock-free metering**: A cache-line-padded SPSC ring buffer transfers
  `LiveMeterResult` structs from the audio thread to the main thread's 30 Hz
  poll timer with `std::memory_order_release`/`acquire` ordering.
- **Static linking**: `libsndfile.a` and `libportaudio.a` (plus 7 codec `.a`
  files: mp3lame, FLAC, vorbisenc, vorbis, ogg, opus, mpg123) are pulled in
  via CMake `INTERFACE` library wrappers.  This eliminates 9 separate DLL files
  from the output directory.
- **UPX safety**: Qt plugin DLLs (`platforms/`, `imageformats/`, `styles/`)
  must NOT be UPX-compressed — the Qt plugin loader depends on specific PE
  structure that UPX modifies.  Only top-level EXEs and DLLs are compressed.
- **WIN32 subsystem**: The GUI executable is built with the `WIN32` flag so
  Windows does not spawn a console window alongside the GUI.  The CLI
  executable uses the default `CONSOLE` subsystem.
