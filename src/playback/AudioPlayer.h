#pragma once

#include "PlayerState.h"
#include "core/AudioFormat.h"
#include "core/RingBuffer.h"
#include "core/LiveMeter.h"

#include <QObject>
#include <QTimer>

#include <atomic>
#include <memory>
#include <string>

class SndfileDecoder;
struct CallbackData;

// PortAudio-backed audio player with real-time loudness metering.
//
// Threading model:
//   Main thread  — owns the QObject, calls load/play/pause/stop/seek,
//                  polls the meter ring buffer at 30Hz.
//   Audio thread — PortAudio callback: decoder.readFrames → LiveMeter.process
//                  → ringBuffer.write → PortAudio output.
//                  Real-time safe: no alloc, no lock, no throw, no Qt signals.
class AudioPlayer : public QObject {
    Q_OBJECT

public:
    using MeterBuffer = RingBuffer<LiveMeterResult, 64>;

    AudioPlayer(QObject* parent = nullptr);
    ~AudioPlayer() override;

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    bool load(const std::string& path);
    void play();
    void pause();
    void stop();
    void seek(int64_t frame);
    void resetMeter();

    PlayerState state() const;
    const AudioFormat& format() const;
    MeterBuffer& meterBuffer();
    int64_t currentFrame() const;

signals:
    void stateChanged(PlayerState state);
    void positionChanged(int64_t frame, float seconds);
    void errorOccurred(const QString& msg);

private slots:
    void onPollTimer();

private:
    void setState(PlayerState s);
    void closeStream();

    std::unique_ptr<SndfileDecoder> decoder_;
    std::unique_ptr<LiveMeter> liveMeter_;
    std::unique_ptr<MeterBuffer> meterBuffer_;

    // PortAudio stream (opaque pointer — avoid including portaudio.h here)
    void* paStream_ = nullptr;

    // Callback user data — heap-allocated, lives as long as the PA stream
    std::unique_ptr<CallbackData> cbData_;

    QTimer* pollTimer_ = nullptr;

    PlayerState state_ = PlayerState::Stopped;

    // Shared with audio callback (real-time safe atomics)
    std::atomic<int64_t> currentFrame_{0};
    std::atomic<bool> callbackError_{false};

    bool paInitialized_ = false;
};
