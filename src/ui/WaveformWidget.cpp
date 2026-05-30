#include "WaveformWidget.h"
#include "core/SndfileDecoder.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QResizeEvent>

#include <algorithm>
#include <cmath>

WaveformWidget::WaveformWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(40);
    setMaximumHeight(56);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMouseTracking(true);
}

bool WaveformWidget::load(const std::string& path)
{
    clear();

    SndfileDecoder decoder;
    if (!decoder.open(path))
        return false;

    const AudioFormat& fmt = decoder.format();
    sampleRate_ = fmt.sampleRate;
    int channels = fmt.channels;

    // Decode entire file in blocks, storing peak of each block
    std::vector<float> buf(kBlockSize * channels);
    for (;;) {
        int64_t read = decoder.readFrames(buf.data(), kBlockSize);
        if (read <= 0)
            break;

        // Find max absolute sample across all channels in this block
        float peak = 0.0f;
        for (int64_t i = 0; i < read * channels; ++i)
            peak = std::max(peak, std::fabs(buf[i]));
        fullPeaks_.push_back(peak);
    }

    if (fullPeaks_.empty())
        return false;

    totalSeconds_ = static_cast<float>(fullPeaks_.size() * kBlockSize) / sampleRate_;
    rebuildPeaks();
    update();
    return true;
}

void WaveformWidget::clear()
{
    fullPeaks_.clear();
    displayPeaks_.clear();
    totalSeconds_ = 0.0f;
    cursorPosition_ = -1.0f;
    sampleRate_ = 0;
    update();
}

void WaveformWidget::setPlaybackPosition(float seconds)
{
    cursorPosition_ = seconds;
    update();
}

void WaveformWidget::rebuildPeaks()
{
    displayPeaks_.clear();
    if (fullPeaks_.empty())
        return;

    int w = width();
    if (w <= 0)
        return;

    // Downsample: each display pixel covers a range of full peaks
    displayPeaks_.resize(w, 0.0f);
    size_t n = fullPeaks_.size();
    for (int i = 0; i < w; ++i) {
        size_t start = static_cast<size_t>(i) * n / static_cast<size_t>(w);
        size_t end = (static_cast<size_t>(i) + 1) * n / static_cast<size_t>(w);
        if (end > n)
            end = n;
        if (start >= end) {
            displayPeaks_[i] = 0.0f;
            continue;
        }
        float maxPeak = 0.0f;
        for (size_t j = start; j < end; ++j)
            maxPeak = std::max(maxPeak, fullPeaks_[j]);
        displayPeaks_[i] = maxPeak;
    }
}

void WaveformWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int midY = h / 2;
    const int maxAmp = midY - 2; // small margin

    // Background
    p.fillRect(0, 0, w, h, QColor(0x2D, 0x2D, 0x2D));

    if (displayPeaks_.empty())
        return;

    // Build upper and lower polygon points for symmetric waveform
    QPolygonF poly;
    poly.reserve(static_cast<int>(displayPeaks_.size()) * 2 + 2);

    // Top edge: left to right
    for (size_t i = 0; i < displayPeaks_.size(); ++i) {
        float amp = displayPeaks_[i] * maxAmp;
        poly << QPointF(static_cast<float>(i), midY - amp);
    }
    // Bottom edge: right to left (mirrored)
    for (size_t i = displayPeaks_.size(); i > 0; --i) {
        float amp = displayPeaks_[i - 1] * maxAmp;
        poly << QPointF(static_cast<float>(i - 1), midY + amp);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x21, 0x96, 0xF3, 0x60)); // Material Blue semi-transparent
    p.drawPolygon(poly);

    // Center line
    p.setPen(QPen(QColor(0x60, 0x60, 0x60), 1));
    p.drawLine(0, midY, w, midY);

    // Playback cursor
    if (cursorPosition_ >= 0.0f && totalSeconds_ > 0.0f) {
        float frac = cursorPosition_ / totalSeconds_;
        int cx = static_cast<int>(frac * w);
        p.setPen(QPen(Qt::white, 2));
        p.drawLine(cx, 0, cx, h);
    }
}

void WaveformWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    rebuildPeaks();
}

void WaveformWidget::mousePressEvent(QMouseEvent* event)
{
    if (totalSeconds_ <= 0.0f)
        return;

    float frac = static_cast<float>(event->position().x()) / width();
    frac = std::clamp(frac, 0.0f, 1.0f);
    emit seekRequested(frac * totalSeconds_);
}

void WaveformWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;
    if (totalSeconds_ <= 0.0f)
        return;

    float frac = static_cast<float>(event->position().x()) / width();
    frac = std::clamp(frac, 0.0f, 1.0f);
    emit seekRequested(frac * totalSeconds_);
}
