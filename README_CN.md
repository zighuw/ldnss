# ldnss — 音频响度分析器

一款 Windows (MSYS2/MinGW) 桌面应用程序，用于离线与实时的音频响度分析，
符合 **ITU-R BS.1770-4**（EBU R128）标准。同时提供图形界面和命令行工具。

## 功能特性

- **离线分析** — 集成LUFS、真峰值（过采样）、瞬时最大值、短时最大值和响度范围（LRA），支持进度回调
- **实时电平监测** — 播放期间通过 PortAudio 实时显示真峰值、瞬时LUFS和短时LUFS
- **波形显示** — 基于降采样峰值包络的可点击波形，支持拖动定位
- **时序图表** — 最近一次分析的瞬时和短时LUFS历史曲线（Qt Charts）
- **拖放支持** — 支持 WAV、FLAC、Ogg、MP3、AIFF 格式
- **最近文件** — 通过 QSettings 持久化保存最近 10 个文件
- **静态链接** — libsndfile、7 个编解码器和 PortAudio 全部编译进程序（除 Qt 和 MSYS2 运行时外无需额外 DLL）
- **UPX 压缩** — EXE 和非插件 DLL 压缩率约 55-60%（UPX NRV）；插件 DLL 保持未压缩以确保 Qt 加载器兼容性

## 架构

三个静态库构成严格的依赖链：

```
┌─────────────────────────────────────────────────┐
│          Qt6 GUI  (ldnss.exe, WIN32)             │
│  Qt6::Widgets + Qt6::Charts                     │
├─────────────────────────────────────────────────┤
│       播放 (ldnss_playback)                      │
│  PortAudio + Qt6::Core (QObject 信号/槽)         │
├─────────────────────────────────────────────────┤
│          核心 (ldnss_core)                        │
│  libsndfile + libebur128 — 纯C++，零Qt依赖        │
└─────────────────────────────────────────────────┘
```

三条运行时线程：
1. **主线程** — Qt 事件循环、GUI 更新、以 30 Hz 从无锁环形缓冲区轮询电平数据
2. **工作线程**（QThread）— 离线 `LoudnessAnalyzer`，通过队列信号通信
3. **音频线程**（PortAudio 回调）— 实时关键路径：无内存分配、无锁、无异常抛出、无 Qt 信号发射

## 依赖项

编译时（MSYS2 `pacman`）：

```
mingw-w64-ucrt-x86_64-{gcc,cmake,ninja}
mingw-w64-ucrt-x86_64-qt6-base
mingw-w64-ucrt-x86_64-qt6-charts
mingw-w64-ucrt-x86_64-libsndfile
mingw-w64-ucrt-x86_64-portaudio
mingw-w64-ucrt-x86_64-libebur128
```

`libebur128` 未提供 `.pc` 文件，因此在 `cmake/` 中包含了自定义的 `FindLibEbur128.cmake` 模块。

运行时（包含在 `output/` 中）：
- 6 个 Qt DLL（由 `windeployqt` 部署）
- 4 个 Qt 插件 DLL（`platforms/`、`imageformats/`、`styles/`）
- 24 个 MSYS2/ucrt64 运行时 DLL
- libsndfile 和 PortAudio **已静态链接** — 无需单独的 DLL

## 编译

```bash
./build.sh                 # cmake 配置 + 编译 + 测试
```

或手动执行：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## 打包

```bash
./package.sh               # 部署 + UPX + 清理
./package.sh --no-clean    # 保留 build/ 目录
```

两个脚本将流水线拆分为编译和打包两个阶段：

**build.sh** (`./build.sh`)：

| 步骤 | 操作 |
|------|------|
| 1 | CMake 配置（Release, Ninja） |
| 2 | 构建 34 个目标 |
| 3 | 运行 3 个测试套件 |

**package.sh** (`./package.sh`) — 需要先完成编译：

| 步骤 | 操作 |
|------|------|
| 1 | 准备 `output/` 目录 |
| 2 | 复制可执行文件 |
| 3 | `windeployqt` + 移除未使用的插件 |
| 4 | 复制 24 个非 Qt 运行时 DLL |
| 5 | UPX `--best`（NRV）压缩 EXE 和非插件 DLL |
| 6 | 清理 `build/` |

最终输出：**36 个文件，约 36 MB**，位于 `output/` 目录。

## 使用说明

### GUI

```bash
output/ldnss.exe
```

- 拖放音频文件到窗口或点击 **Browse…** 按钮
- 播放控制：播放/暂停、停止、定位滑块、重置电平表
- 点击波形图可跳转播放位置
- 播放期间实时更新电平表
- 离线分析结果显示在右侧面板中，并附带时序图表
- 最近文件跨会话持久化保存（文件 → 最近文件）

### CLI

```bash
output/ldnss-cli.exe --analyze <file>   # 离线分析
output/ldnss-cli.exe --play <file>      # 播放并实时监测
output/ldnss-cli.exe --version
output/ldnss-cli.exe --help
```

**`--analyze` 输出示例：**

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

**`--play` 输出示例：**

```
Playing music.wav (44100 Hz, 2 ch, 197.4 s)
  TP: +0.5 dBTP  |  St: -13.1 LUFS  |  Mo: -11.2 LUFS
  TP: +0.6 dBTP  |  St: -13.0 LUFS  |  Mo: -11.0 LUFS
```

## 项目结构

```
ldnss/
├── CMakeLists.txt              # 顶层：项目配置、C++17、AUTOMOC
├── ARCHITECTURE.md             # 详细设计原理
├── README.md
├── build.sh                    # cmake 配置 + 编译 + 测试
├── package.sh                  # 部署 + UPX + 清理
├── cmake/
│   └── FindLibEbur128.cmake    # 自定义 find-module（无 .pc 文件）
├── src/
│   ├── CMakeLists.txt          # 4 个目标：core、playback、CLI、GUI
│   ├── main.cpp                # CLI 入口点
│   ├── main_gui.cpp            # GUI 入口点
│   ├── core/                   # 纯 C++ DSP（无 Qt / PortAudio）
│   │   ├── AudioFile.h         # 抽象解码器接口
│   │   ├── AudioFormat.h       # 采样率 / 声道数 / 帧数
│   │   ├── SndfileDecoder.h/cpp
│   │   ├── LoudnessAnalyzer.h/cpp
│   │   ├── LoudnessResult.h
│   │   ├── LiveMeter.h/cpp
│   │   ├── TruePeakMeter.h/cpp  # 自定义 4× 过采样器（Kaiser 滤波器）
│   │   └── RingBuffer.h        # 无锁 SPSC 队列
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
    ├── test_audiofile.cpp      # SndfileDecoder 打开/读取/定位
    ├── test_loudness.cpp       # LoudnessAnalyzer 参考信号
    └── test_livemeter.cpp      # LiveMeter 实时块处理
```

## 测试

通过 CTest 运行 3 个测试可执行文件：

| 测试 | 验证内容 |
|------|----------|
| `test_audiofile` | 文件打开失败、单声道/立体声解码、定位、格式读取 |
| `test_loudness` | EBU Tech 3341 参考音（-23 LUFS）、立体声加权（+3 dB）、静音鲁棒性 |
| `test_livemeter` | 实时块处理、立体声增益、静音底噪、重置、移动语义 |

测试通过 libsndfile 程序化生成 WAV 文件 — 无需测试素材。

## 技术说明

- **EBU R128 合规性**：离线分析使用 `libebur128`，模式为
  `EBUR128_MODE_I | LRA | TRUE_PEAK`，用于综合 LUFS、LRA 和内部真峰值；
  实时监测使用 `EBUR128_MODE_S`，仅用于同时测量瞬时（400 ms）和短时（3 s）LUFS。
- **自定义真峰值过采样器**（`TruePeakMeter`）：采用双精度 128 抽头
  Kaiser 窗多相 FIR 滤波器（4× 过采样，β=8，~80 dB 阻带衰减），替代
  libebur128 的浮点精度 49 抽头 Hamming FIR，用于最终的 dBTP 读数。
  所有分析全程使用双精度以最小化累积舍入噪声。
- **实时安全性**：PortAudio 回调仅调用 `decoder->readFrames`、
  `meter->process` 和 `ringBuffer->write`。无堆分配、无锁获取、
  无异常抛出、无 Qt 信号发射。
- **无锁电平传输**：采用缓存行填充的 SPSC 环形缓冲区，以
  `std::memory_order_release`/`acquire` 顺序将 `LiveMeterResult` 结构体
  从音频线程传输至主线程的 30 Hz 轮询定时器。
- **静态链接**：通过 CMake `INTERFACE` 库封装 `libsndfile.a` 和
  `libportaudio.a`（外加 7 个编解码器 `.a` 文件：mp3lame、FLAC、vorbisenc、
  vorbis、ogg、opus、mpg123）。这从输出目录中消除了 9 个独立的 DLL 文件。
- **UPX 安全性**：压缩 EXE 和非插件 DLL 文件（UPX `--best`，NRV 算法）。
  `platforms/`、`imageformats/`、`styles/` 中的插件 DLL 保持未压缩 —
  UPX 压缩可能破坏 Qt 插件加载器依赖的 PE 结构。同时避免使用 LZMA
  压缩，因其在 UPX 5.1.1 下曾产生损坏的 EXE（STATUS_DLL_INIT_FAILED）。
- **WIN32 子系统**：GUI 可执行文件使用 `WIN32` 标志构建，使 Windows
  不会在 GUI 窗口旁弹出控制台窗口。CLI 可执行文件使用默认的 `CONSOLE` 子系统。
