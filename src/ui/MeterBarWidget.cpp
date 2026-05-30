#include "MeterBarWidget.h"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>

static const QColor kFillColor(0x21, 0x96, 0xF3); // single Meter Blue fill

MeterBarWidget::MeterBarWidget(MeterType type, QWidget* parent)
    : QWidget(parent)
    , type_(type)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

void MeterBarWidget::setValue(float db)
{
    currentValue_ = db;
    if (db > maxValue_)
        maxValue_ = db;
    update();
}

void MeterBarWidget::setTitle(const QString& title)
{
    title_ = title;
    update();
}

void MeterBarWidget::reset()
{
    currentValue_ = -120.0f;
    maxValue_ = -120.0f;
    update();
}

QSize MeterBarWidget::minimumSizeHint() const
{
    const int w = barWidth_ + 52;  // bar + left tick margin + right padding
    const int h = 150;             // flexible — bar scales to 80% of actual height
    return {w, h};
}

QSize MeterBarWidget::sizeHint() const
{
    return minimumSizeHint();
}

void MeterBarWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int totalW = width();
    const int totalH = height();
    const int leftMargin = 42; // space for dB labels
    const int barX = leftMargin;
    const int barTop = 20;     // room for max-value label above
    const int bottomGap = 56;  // range labels + unit + title
    const int maxBarH = totalH - barTop - bottomGap;
    const int barH = std::min(static_cast<int>(totalH * 0.8f), maxBarH);
    const QRect barRect(barX, barTop, barWidth_, barH);

    // --- Background (empty bar) ---
    p.setPen(QPen(QColor(0x60, 0x60, 0x60), 1));
    p.setBrush(QColor(0x3A, 0x3A, 0x3A));
    p.drawRect(barRect);

    // --- Filled portion ---
    float clamped = std::clamp(currentValue_, displayMin_, displayMax_);
    float fraction = (clamped - displayMin_) / (displayMax_ - displayMin_);
    int fillHeight = static_cast<int>(fraction * barH);

    if (fillHeight > 0) {
        QRect fillRect(barX + 1, barTop + barH - fillHeight,
                       barWidth_ - 2, fillHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(kFillColor);
        p.drawRect(fillRect);
    }

    // --- EBU threshold reference lines (subtle horizontal dashed lines) ---
    auto drawThreshold = [&](float dbValue, const QColor& color) {
        float frac = (dbValue - displayMin_) / (displayMax_ - displayMin_);
        int y = barTop + barH - static_cast<int>(frac * barH);
        p.setPen(QPen(color, 1, Qt::DashLine));
        p.drawLine(barX + 1, y, barX + barWidth_ - 2, y);
    };

    if (type_ == MeterType::LUFS) {
        drawThreshold(-23.0f, QColor(0x4C, 0xAF, 0x50, 0x60));
        drawThreshold(-18.0f, QColor(0xF4, 0x43, 0x36, 0x60));
    } else {
        drawThreshold(-3.0f, QColor(0x4C, 0xAF, 0x50, 0x60));
        drawThreshold(-1.0f, QColor(0xF4, 0x43, 0x36, 0x60));
    }

    // --- Vertical scale ticks on the left side ---
    QFont scaleFont(QStringLiteral("Consolas"), 7);
    p.setFont(scaleFont);

    for (int dbVal = -50; dbVal <= 5; dbVal += 5) {
        float frac = (static_cast<float>(dbVal) - displayMin_)
                     / (displayMax_ - displayMin_);
        int y = barTop + barH - static_cast<int>(frac * barH);

        bool isMajor = (dbVal % 10 == 0);

        if (isMajor) {
            // Major tick + numeric label every 10 dB
            p.setPen(QPen(QColor(0xB0, 0xB0, 0xB0), 1));
            p.drawLine(barX - 10, y, barX, y);
            p.setPen(QColor(0x90, 0x90, 0x90));
            p.drawText(QRect(0, y - 8, barX - 12, 16),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(dbVal));
        } else {
            // Minor tick every 5 dB (no label)
            p.setPen(QPen(QColor(0x60, 0x60, 0x60), 1));
            p.drawLine(barX - 6, y, barX, y);
        }
    }

    // --- Max-value label (above bar) ---
    QFont valFont(QStringLiteral("Consolas"), 11, QFont::Bold);
    p.setFont(valFont);
    p.setPen(QColor(0xF4, 0x43, 0x36)); // red
    QString maxText;
    if (maxValue_ <= -120.0f)
        maxText = QStringLiteral("\u2014"); // em dash for silence
    else
        maxText = QStringLiteral("MAX %1").arg(
            QString::asprintf("%+.1f", static_cast<double>(maxValue_)));
    p.drawText(QRect(0, 0, totalW, barTop), Qt::AlignCenter, maxText);

    // --- Range labels (below bar) ---
    QFont labelFont(QStringLiteral("Consolas"), 8);
    p.setFont(labelFont);
    p.setPen(QColor(0xB0, 0xB0, 0xB0));

    // Min / Max labels
    p.drawText(QRect(barX - 4, barTop + barH + 2, 30, 16),
               Qt::AlignLeft | Qt::AlignTop, QString::number(static_cast<int>(displayMin_)));
    p.drawText(QRect(barX - 4, barTop + barH + 2, barWidth_ + 8, 16),
               Qt::AlignRight | Qt::AlignTop, QString::number(static_cast<int>(displayMax_)));

    // Unit
    const char* unit = (type_ == MeterType::TruePeak) ? "dBTP" : "LUFS";
    p.drawText(QRect(barX - 4, barTop + barH + 18, barWidth_ + 8, 16),
               Qt::AlignCenter, QString::fromLatin1(unit));

    // --- Title (below unit) ---
    QFont titleFont(QStringLiteral("Segoe UI"), 9);
    p.setFont(titleFont);
    p.setPen(QColor(0xD0, 0xD0, 0xD0));
    p.drawText(QRect(0, barTop + barH + 36, totalW, 20),
               Qt::AlignCenter, title_);
}
