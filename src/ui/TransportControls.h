#pragma once

#include "playback/PlayerState.h"

#include <QWidget>

class QPushButton;
class QSlider;
class QLabel;

// Play/Pause/Stop buttons + seek slider + time labels.
// Handles slider-drag vs position-update conflict internally via isSeeking_ flag.
class TransportControls : public QWidget {
    Q_OBJECT

public:
    explicit TransportControls(QWidget* parent = nullptr);

    void setTotalDuration(float seconds);

signals:
    void playRequested();
    void pauseRequested();
    void stopRequested();
    void seekRequested(float seconds);
    void resetMetersRequested();

public slots:
    void setState(PlayerState state);
    void setPosition(int64_t frame, float seconds);

private slots:
    void onPlayPauseClicked();
    void onStopClicked();
    void onSliderPressed();
    void onSliderReleased();

private:
    static QString formatTime(float seconds);

    QPushButton* playPauseBtn_;
    QPushButton* stopBtn_;
    QPushButton* resetMetersBtn_;
    QLabel* currentTimeLabel_;
    QLabel* totalTimeLabel_;
    QSlider* seekSlider_;

    PlayerState state_ = PlayerState::Stopped;
    float totalSeconds_ = 0.0f;
    bool isSeeking_ = false;
};
