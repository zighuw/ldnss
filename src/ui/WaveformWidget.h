#pragma once

#include <QWidget>

#include <string>
#include <vector>

// Time-domain waveform rendering with peak envelope data.
// Decodes the file once into a peak envelope, then downsamples to widget width
// on resize. Supports click-to-seek and a playback position cursor.
class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget* parent = nullptr);

    bool load(const std::string& path);
    void clear();

    float totalDuration() const { return totalSeconds_; }

public slots:
    void setPlaybackPosition(float seconds);

signals:
    void seekRequested(float seconds);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void rebuildPeaks();

    std::vector<float> fullPeaks_;    // one peak per 256-frame block
    std::vector<float> displayPeaks_; // downsampled to width()
    float totalSeconds_ = 0.0f;
    float cursorPosition_ = -1.0f;    // -1 means hidden
    int sampleRate_ = 0;

    static constexpr int kBlockSize = 256;
    static constexpr int kMinBarWidth = 2; // minimum pixels per display peak
};
