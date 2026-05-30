#pragma once

#include <QWidget>

// A single custom-painted vertical meter bar with a single fill colour,
// vertical scale ticks on the left, and a peak-hold (maximum) readout
// at the top.
//
// Two measurement modes:
//   LUFS     — scale -50 … +5  dB (LUFS)
//   TruePeak — scale -50 … +5  dB (dBTP)
class MeterBarWidget : public QWidget {
    Q_OBJECT

public:
    enum class MeterType {
        LUFS,
        TruePeak,
    };

    explicit MeterBarWidget(MeterType type, QWidget* parent = nullptr);

    void setValue(float db);
    void setTitle(const QString& title);
    void reset();

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    MeterType type_;
    float currentValue_ = -120.0f;
    float maxValue_ = -120.0f;     // peak hold since last reset
    QString title_;

    static constexpr float displayMin_ = -50.0f;
    static constexpr float displayMax_ = 5.0f;
    static constexpr int barWidth_ = 56;
    static constexpr int barHeight_ = 400;
};
