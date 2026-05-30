#include "LiveMeterPanel.h"
#include "MeterBarWidget.h"
#include "playback/AudioPlayer.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

LiveMeterPanel::LiveMeterPanel(QWidget* parent)
    : QGroupBox(QStringLiteral("Live Meters"), parent)
    , pollTimer_(new QTimer(this))
{
    auto* layout = new QHBoxLayout(this);

    auto makeColumn = [&](const QString& title, MeterBarWidget::MeterType type,
                           MeterBarWidget*& barOut, QLabel*& labelOut) {
        auto* col = new QVBoxLayout;
        col->setAlignment(Qt::AlignCenter);

        barOut = new MeterBarWidget(type);
        barOut->setTitle(title);
        col->addWidget(barOut);

        labelOut = new QLabel(QStringLiteral("\u2014")); // em dash
        labelOut->setAlignment(Qt::AlignCenter);
        labelOut->setFixedWidth(110);
        QFont labelFont(QStringLiteral("Consolas"), 12, QFont::Bold);
        labelOut->setFont(labelFont);
        labelOut->setStyleSheet(QStringLiteral("QLabel { color: #2D2D2D; }"));
        col->addWidget(labelOut);

        layout->addLayout(col);
    };

    makeColumn(QStringLiteral("True Peak"), MeterBarWidget::MeterType::TruePeak,
               truePeakBar_, truePeakLabel_);
    makeColumn(QStringLiteral("Short-term"), MeterBarWidget::MeterType::LUFS,
               shortTermBar_, shortTermLabel_);
    makeColumn(QStringLiteral("Momentary"), MeterBarWidget::MeterType::LUFS,
               momentaryBar_, momentaryLabel_);

    layout->addStretch();

    pollTimer_->setInterval(33); // ~30 Hz
    connect(pollTimer_, &QTimer::timeout, this, &LiveMeterPanel::onPollTimer);
}

void LiveMeterPanel::setPlayer(AudioPlayer* player)
{
    player_ = player;
    if (player_) {
        pollTimer_->start();
    } else {
        pollTimer_->stop();
    }
}

void LiveMeterPanel::reset()
{
    truePeakBar_->reset();
    shortTermBar_->reset();
    momentaryBar_->reset();
    truePeakLabel_->setText(QStringLiteral("\u2014"));
    shortTermLabel_->setText(QStringLiteral("\u2014"));
    momentaryLabel_->setText(QStringLiteral("\u2014"));
}

void LiveMeterPanel::onPollTimer()
{
    if (!player_)
        return;

    LiveMeterResult result;
    while (player_->meterBuffer().readLatest(result)) {
        // drain all available updates, keep the latest
    }

    truePeakBar_->setValue(result.truePeakDB);
    shortTermBar_->setValue(result.shortTermLUFS);
    momentaryBar_->setValue(result.momentaryLUFS);

    auto formatDB = [](float db) -> QString {
        if (db <= -120.0f)
            return QStringLiteral("\u2014");
        return QString::asprintf("%+.1f", static_cast<double>(db));
    };

    truePeakLabel_->setText(formatDB(result.truePeakDB) + QStringLiteral(" dBTP"));
    shortTermLabel_->setText(formatDB(result.shortTermLUFS) + QStringLiteral(" LUFS"));
    momentaryLabel_->setText(formatDB(result.momentaryLUFS) + QStringLiteral(" LUFS"));
}
