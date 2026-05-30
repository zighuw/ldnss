#include "TransportControls.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>

TransportControls::TransportControls(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);

    playPauseBtn_ = new QPushButton(QStringLiteral("\u25B6")); // ▶
    playPauseBtn_->setFixedWidth(36);
    playPauseBtn_->setToolTip(QStringLiteral("Play"));
    layout->addWidget(playPauseBtn_);

    stopBtn_ = new QPushButton(QStringLiteral("\u25A0")); // ■
    stopBtn_->setFixedWidth(36);
    stopBtn_->setToolTip(QStringLiteral("Stop"));
    layout->addWidget(stopBtn_);

    resetMetersBtn_ = new QPushButton(QStringLiteral("\u21BA")); // ↺
    resetMetersBtn_->setFixedWidth(36);
    resetMetersBtn_->setToolTip(QStringLiteral("Reset meters"));
    resetMetersBtn_->setEnabled(false);
    layout->addWidget(resetMetersBtn_);

    currentTimeLabel_ = new QLabel(QStringLiteral("0:00"));
    currentTimeLabel_->setFixedWidth(48);
    currentTimeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(currentTimeLabel_);

    seekSlider_ = new QSlider(Qt::Horizontal);
    seekSlider_->setRange(0, 1000);
    seekSlider_->setValue(0);
    seekSlider_->setEnabled(false);
    layout->addWidget(seekSlider_, 1);

    totalTimeLabel_ = new QLabel(QStringLiteral("0:00"));
    totalTimeLabel_->setFixedWidth(48);
    totalTimeLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(totalTimeLabel_);

    connect(playPauseBtn_, &QPushButton::clicked,
            this, &TransportControls::onPlayPauseClicked);
    connect(stopBtn_, &QPushButton::clicked,
            this, &TransportControls::onStopClicked);
    connect(resetMetersBtn_, &QPushButton::clicked,
            this, &TransportControls::resetMetersRequested);
    connect(seekSlider_, &QSlider::sliderPressed,
            this, &TransportControls::onSliderPressed);
    connect(seekSlider_, &QSlider::sliderReleased,
            this, &TransportControls::onSliderReleased);

    setState(PlayerState::Stopped);
}

void TransportControls::setTotalDuration(float seconds)
{
    totalSeconds_ = seconds;
    totalTimeLabel_->setText(formatTime(seconds));
    seekSlider_->setEnabled(seconds > 0.0f);
}

void TransportControls::setState(PlayerState state)
{
    state_ = state;

    switch (state) {
    case PlayerState::Stopped:
        playPauseBtn_->setText(QStringLiteral("\u25B6")); // ▶
        playPauseBtn_->setToolTip(QStringLiteral("Play"));
        playPauseBtn_->setEnabled(true);
        stopBtn_->setEnabled(false);
        resetMetersBtn_->setEnabled(totalSeconds_ > 0.0f);
        seekSlider_->setEnabled(totalSeconds_ > 0.0f);
        break;
    case PlayerState::Playing:
        playPauseBtn_->setText(QStringLiteral("\u23F8")); // ⏸
        playPauseBtn_->setToolTip(QStringLiteral("Pause"));
        playPauseBtn_->setEnabled(true);
        stopBtn_->setEnabled(true);
        resetMetersBtn_->setEnabled(true);
        seekSlider_->setEnabled(true);
        break;
    case PlayerState::Paused:
        playPauseBtn_->setText(QStringLiteral("\u25B6")); // ▶
        playPauseBtn_->setToolTip(QStringLiteral("Resume"));
        playPauseBtn_->setEnabled(true);
        stopBtn_->setEnabled(true);
        resetMetersBtn_->setEnabled(true);
        seekSlider_->setEnabled(true);
        break;
    case PlayerState::Error:
        playPauseBtn_->setEnabled(false);
        stopBtn_->setEnabled(false);
        resetMetersBtn_->setEnabled(false);
        seekSlider_->setEnabled(false);
        break;
    }
}

void TransportControls::setPosition(int64_t /*frame*/, float seconds)
{
    if (isSeeking_)
        return;

    currentTimeLabel_->setText(formatTime(seconds));

    if (totalSeconds_ > 0.0f && !isSeeking_) {
        int sliderVal = static_cast<int>((seconds / totalSeconds_) * 1000.0f);
        seekSlider_->setValue(std::clamp(sliderVal, 0, 1000));
    }
}

void TransportControls::onPlayPauseClicked()
{
    if (state_ == PlayerState::Playing)
        emit pauseRequested();
    else
        emit playRequested();
}

void TransportControls::onStopClicked()
{
    emit stopRequested();
}

void TransportControls::onSliderPressed()
{
    isSeeking_ = true;
}

void TransportControls::onSliderReleased()
{
    isSeeking_ = false;
    if (totalSeconds_ > 0.0f) {
        float seconds = (seekSlider_->value() / 1000.0f) * totalSeconds_;
        emit seekRequested(seconds);
    }
}

QString TransportControls::formatTime(float seconds)
{
    if (seconds < 0.0f)
        seconds = 0.0f;

    int totalSec = static_cast<int>(seconds);
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;

    if (h > 0)
        return QString::asprintf("%d:%02d:%02d", h, m, s);
    else
        return QString::asprintf("%d:%02d", m, s);
}
