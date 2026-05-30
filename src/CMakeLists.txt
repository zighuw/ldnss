# --- Core library (pure C++, zero Qt/PortAudio) ---
add_library(ldnss_core STATIC
    core/SndfileDecoder.cpp
    core/LoudnessAnalyzer.cpp
    core/LiveMeter.cpp
    core/TruePeakMeter.cpp
)

target_include_directories(ldnss_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)

find_package(LibEbur128 REQUIRED)

# Static libsndfile + codecs (eliminates libsndfile-1.dll and 7 codec DLLs)
set(SNDFILE_STATIC_LIB_DIR "${SNDFILE_LIBRARY_DIRS}")
# pkg-config may return "D:/.../bin/../lib" — normalize to "D:/.../lib"
get_filename_component(SNDFILE_STATIC_LIB_DIR "${SNDFILE_STATIC_LIB_DIR}" ABSOLUTE)

add_library(sndfile_static INTERFACE)
target_link_libraries(sndfile_static INTERFACE
    "${SNDFILE_STATIC_LIB_DIR}/libsndfile.a"
    "${SNDFILE_STATIC_LIB_DIR}/libmp3lame.a"
    "${SNDFILE_STATIC_LIB_DIR}/libFLAC.a"
    "${SNDFILE_STATIC_LIB_DIR}/libvorbisenc.a"
    "${SNDFILE_STATIC_LIB_DIR}/libvorbis.a"
    "${SNDFILE_STATIC_LIB_DIR}/libogg.a"
    "${SNDFILE_STATIC_LIB_DIR}/libopus.a"
    "${SNDFILE_STATIC_LIB_DIR}/libmpg123.a"
    shlwapi  # mpg123 needs PathIsRelativeW/PathIsUNCW/PathCombineW
    winmm    # some codec backends need waveOut events
)
target_include_directories(sndfile_static INTERFACE ${SNDFILE_INCLUDE_DIRS})

target_link_libraries(ldnss_core
    PUBLIC
        sndfile_static
        LibEbur128::LibEbur128
)

target_include_directories(ldnss_core SYSTEM PUBLIC ${SNDFILE_INCLUDE_DIRS})
target_compile_options(ldnss_core PRIVATE ${SNDFILE_CFLAGS_OTHER})

# --- Playback library (PortAudio + Qt6::Core) ---
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts)

add_library(ldnss_playback STATIC
    playback/AudioPlayer.cpp
)

target_include_directories(ldnss_playback PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(ldnss_playback PRIVATE ${PORTAUDIO_CFLAGS_OTHER})

# Static portaudio (eliminates libportaudio.dll)
set(PORTAUDIO_STATIC_LIB_DIR "${PORTAUDIO_LIBRARY_DIRS}")
get_filename_component(PORTAUDIO_STATIC_LIB_DIR "${PORTAUDIO_STATIC_LIB_DIR}" ABSOLUTE)

add_library(portaudio_static INTERFACE)
target_link_libraries(portaudio_static INTERFACE
    "${PORTAUDIO_STATIC_LIB_DIR}/libportaudio.a"
    winmm      # waveOut/waveIn/timeGetTime/timeBeginPeriod
    setupapi   # SetupDi* device enumeration (WASAPI/WDM-KS backends)
)
target_include_directories(portaudio_static INTERFACE ${PORTAUDIO_INCLUDE_DIRS})

target_link_libraries(ldnss_playback
    PUBLIC
        ldnss_core
        portaudio_static
        Qt6::Core
)

# --- CLI executable ---
add_executable(ldnss-cli main.cpp)
target_link_libraries(ldnss-cli PRIVATE ldnss_core ldnss_playback)

# --- GUI executable ---
add_executable(ldnss WIN32
    main_gui.cpp
    ui/AnalysisWorker.cpp
    ui/WorkerController.cpp
    ui/AnalysisPanel.cpp
    ui/LoudnessChartWidget.cpp
    ui/FileBrowserWidget.cpp
    ui/MainWindow.cpp
    ui/MeterBarWidget.cpp
    ui/TransportControls.cpp
    ui/WaveformWidget.cpp
    ui/LiveMeterPanel.cpp
)
target_link_libraries(ldnss
    PRIVATE
        ldnss_core
        ldnss_playback
        Qt6::Widgets
        Qt6::Charts
)

# Install
install(TARGETS ldnss-cli ldnss RUNTIME DESTINATION bin)
