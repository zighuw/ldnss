#include "AudioPlayer.h"
#include "core/SndfileDecoder.h"

#include <portaudio.h>

#include <cstring>
#include <memory>
#include <utility>

// ---------------------------------------------------------------------------
// Data shared with the PortAudio callback — all pointed-to objects must
// remain valid while the stream is active.  Atomics are safe from both sides.
// ---------------------------------------------------------------------------
struct CallbackData {
    SndfileDecoder* decoder = nullptr;
    LiveMeter* meter = nullptr;
    AudioPlayer::MeterBuffer* meterBuffer = nullptr;
    std::atomic<int64_t>* currentFrame = nullptr;
    std::atomic<bool>* errorFlag = nullptr;
    int channels = 0;
};

// ---------------------------------------------------------------------------
// PortAudio callback — runs on the real-time audio thread.
// MUST NOT: allocate, lock, throw, emit Qt signals, or call printf.
// ---------------------------------------------------------------------------
static int paCallback(const void* /*input*/,
                      void* output,
                      unsigned long frameCount,
                      const PaStreamCallbackTimeInfo* /*timeInfo*/,
                      PaStreamCallbackFlags /*statusFlags*/,
                      void* userData)
{
    auto* cd = static_cast<CallbackData*>(userData);
    float* out = static_cast<float*>(output);

    int64_t read = cd->decoder->readFrames(out, static_cast<int64_t>(frameCount));

    if (read < 0) {
        cd->errorFlag->store(true, std::memory_order_release);
        std::memset(out, 0, frameCount * cd->channels * sizeof(float));
        return paComplete;
    }

    if (read < static_cast<int64_t>(frameCount)) {
        std::memset(out + read * cd->channels, 0,
                    (static_cast<unsigned long>(frameCount) - read)
                        * cd->channels * sizeof(float));
    }

    if (read > 0) {
        LiveMeterResult result = cd->meter->process(out, static_cast<int>(read));
        cd->meterBuffer->write(result);
        cd->currentFrame->fetch_add(read, std::memory_order_release);
    }

    return (read < static_cast<int64_t>(frameCount)) ? paComplete : paContinue;
}

// ---------------------------------------------------------------------------
// AudioPlayer
// ---------------------------------------------------------------------------

AudioPlayer::AudioPlayer(QObject* parent)
    : QObject(parent)
    , decoder_(new SndfileDecoder)
    , meterBuffer_(new MeterBuffer)
    , pollTimer_(new QTimer(this))
{
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        emit errorOccurred(QString::fromLatin1(Pa_GetErrorText(err)));
        state_ = PlayerState::Error;
    } else {
        paInitialized_ = true;
    }
    pollTimer_->setInterval(33); // ~30 Hz
    connect(pollTimer_, &QTimer::timeout, this, &AudioPlayer::onPollTimer);
}

AudioPlayer::~AudioPlayer()
{
    closeStream();
    if (paInitialized_)
        Pa_Terminate();
}

bool AudioPlayer::load(const std::string& path)
{
    closeStream();
    currentFrame_.store(0, std::memory_order_release);
    callbackError_.store(false, std::memory_order_release);

    if (!decoder_->open(path)) {
        setState(PlayerState::Error);
        emit errorOccurred(QString::fromStdString("Cannot open file: " + path));
        return false;
    }

    const AudioFormat& fmt = decoder_->format();
    liveMeter_.reset(new LiveMeter(fmt.sampleRate, fmt.channels));

    setState(PlayerState::Stopped);
    return true;
}

void AudioPlayer::play()
{
    if (!decoder_->isOpen()) {
        setState(PlayerState::Error);
        emit errorOccurred(QStringLiteral("No file loaded"));
        return;
    }

    if (state_ == PlayerState::Playing)
        return;

    // Resume after pause
    if (state_ == PlayerState::Paused && paStream_) {
        PaError err = Pa_StartStream(static_cast<PaStream*>(paStream_));
        if (err != paNoError) {
            setState(PlayerState::Error);
            emit errorOccurred(QString::fromLatin1(Pa_GetErrorText(err)));
            return;
        }
        setState(PlayerState::Playing);
        return;
    }

    // Fresh start
    closeStream();

    const AudioFormat& fmt = decoder_->format();

    // Allocate callback data — lives as long as the stream
    auto cbData = std::make_unique<CallbackData>();
    cbData->decoder = decoder_.get();
    cbData->meter = liveMeter_.get();
    cbData->meterBuffer = meterBuffer_.get();
    cbData->currentFrame = &currentFrame_;
    cbData->errorFlag = &callbackError_;
    cbData->channels = fmt.channels;

    PaError err = Pa_OpenDefaultStream(
        &paStream_,
        0,                       // no input
        fmt.channels,            // output channels
        paFloat32,               // sample format
        static_cast<double>(fmt.sampleRate),
        paFramesPerBufferUnspecified,
        paCallback,
        cbData.get());           // userData pointer

    if (err != paNoError) {
        setState(PlayerState::Error);
        emit errorOccurred(QString::fromLatin1(Pa_GetErrorText(err)));
        return;
    }

    // Take ownership — PortAudio only stores the raw pointer, so we must
    // keep the CallbackData alive for the lifetime of the stream.
    cbData_ = std::move(cbData);

    err = Pa_StartStream(static_cast<PaStream*>(paStream_));
    if (err != paNoError) {
        closeStream();
        setState(PlayerState::Error);
        emit errorOccurred(QString::fromLatin1(Pa_GetErrorText(err)));
        return;
    }

    setState(PlayerState::Playing);
    pollTimer_->start();
}

void AudioPlayer::pause()
{
    if (state_ != PlayerState::Playing)
        return;

    if (paStream_) {
        Pa_StopStream(static_cast<PaStream*>(paStream_));
    }
    pollTimer_->stop();
    setState(PlayerState::Paused);
}

void AudioPlayer::stop()
{
    if (state_ == PlayerState::Stopped)
        return;

    closeStream();
    pollTimer_->stop();

    // Seek back to beginning
    if (decoder_->isOpen()) {
        decoder_->seekToFrame(0);
        if (liveMeter_)
            liveMeter_->reset();
    }
    currentFrame_.store(0, std::memory_order_release);

    setState(PlayerState::Stopped);
}

void AudioPlayer::seek(int64_t frame)
{
    if (!decoder_->isOpen())
        return;

    PlayerState prevState = state_;
    closeStream();
    pollTimer_->stop();

    decoder_->seekToFrame(frame);
    if (liveMeter_)
        liveMeter_->reset();
    currentFrame_.store(frame, std::memory_order_release);

    if (prevState == PlayerState::Playing) {
        state_ = PlayerState::Stopped; // closeStream() nulled paStream_ but
                                       // state_ is still Playing — force a
                                       // fresh start in play()
        play();
    } else {
        setState(prevState);
    }
}

void AudioPlayer::resetMeter()
{
    if (liveMeter_)
        liveMeter_->reset();
}

PlayerState AudioPlayer::state() const
{
    return state_;
}

const AudioFormat& AudioPlayer::format() const
{
    return decoder_->format();
}

AudioPlayer::MeterBuffer& AudioPlayer::meterBuffer()
{
    return *meterBuffer_;
}

int64_t AudioPlayer::currentFrame() const
{
    return currentFrame_.load(std::memory_order_acquire);
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void AudioPlayer::setState(PlayerState s)
{
    if (state_ == s)
        return;
    state_ = s;
    emit stateChanged(s);
}

void AudioPlayer::closeStream()
{
    if (paStream_) {
        Pa_StopStream(static_cast<PaStream*>(paStream_));
        Pa_CloseStream(static_cast<PaStream*>(paStream_));
        paStream_ = nullptr;
    }
    cbData_.reset();
}

void AudioPlayer::onPollTimer()
{
    // Check for callback errors
    if (callbackError_.load(std::memory_order_acquire)) {
        closeStream();
        pollTimer_->stop();
        setState(PlayerState::Error);
        emit errorOccurred(QStringLiteral("Decode error in audio callback"));
        return;
    }

    // Check if stream is still active (PortAudio stops on paComplete)
    if (paStream_) {
        PaError active = Pa_IsStreamActive(static_cast<PaStream*>(paStream_));
        if (active < 1) {
            // Stream finished (EOF reached)
            closeStream();
            pollTimer_->stop();
            setState(PlayerState::Stopped);
            return;
        }
    }

    // Emit position from atomic counter
    int64_t frame = currentFrame_.load(std::memory_order_acquire);
    if (decoder_->isOpen()) {
        const AudioFormat& fmt = decoder_->format();
        float seconds = (fmt.sampleRate > 0)
            ? static_cast<float>(frame) / static_cast<float>(fmt.sampleRate)
            : 0.0f;
        emit positionChanged(frame, seconds);
    }
}
