#pragma once

#include <QGroupBox>

class AudioPlayer;
class MeterBarWidget;
class QLabel;
class QTimer;

// Composite widget with 3 MeterBarWidgets (True Peak, Short-term LUFS,
// Momentary LUFS) and an internal 30Hz QTimer that polls the AudioPlayer's
// meter ring buffer. Each bar has a numeric readout label below it.
class LiveMeterPanel : public QGroupBox {
    Q_OBJECT

public:
    explicit LiveMeterPanel(QWidget* parent = nullptr);

    void setPlayer(AudioPlayer* player);
    void reset();

private slots:
    void onPollTimer();

private:
    MeterBarWidget* truePeakBar_;
    MeterBarWidget* shortTermBar_;
    MeterBarWidget* momentaryBar_;
    QLabel* truePeakLabel_;
    QLabel* shortTermLabel_;
    QLabel* momentaryLabel_;
    QTimer* pollTimer_;
    AudioPlayer* player_ = nullptr;
};
