#include "AnalysisPanel.h"

#include <QLabel>
#include <QProgressBar>
#include <QString>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFont>

AnalysisPanel::AnalysisPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    // --- Result group ---
    auto* resultGroup = new QGroupBox(QStringLiteral("Loudness Results"), this);
    auto* form = new QFormLayout(resultGroup);

    auto monoFont = QFont(QStringLiteral("Consolas"), 11);
    monoFont.setBold(true);

    integratedLabel_ = new QLabel(QStringLiteral("—"), this);
    integratedLabel_->setFont(monoFont);
    form->addRow(QStringLiteral("Integrated LUFS:"), integratedLabel_);

    truePeakLabel_ = new QLabel(QStringLiteral("—"), this);
    truePeakLabel_->setFont(monoFont);
    form->addRow(QStringLiteral("Max True Peak:"), truePeakLabel_);

    maxMomentaryLabel_ = new QLabel(QStringLiteral("—"), this);
    maxMomentaryLabel_->setFont(monoFont);
    form->addRow(QStringLiteral("Max Momentary:"), maxMomentaryLabel_);

    maxShortTermLabel_ = new QLabel(QStringLiteral("—"), this);
    maxShortTermLabel_->setFont(monoFont);
    form->addRow(QStringLiteral("Max Short-term:"), maxShortTermLabel_);

    lraLabel_ = new QLabel(QStringLiteral("—"), this);
    lraLabel_->setFont(monoFont);
    form->addRow(QStringLiteral("Loudness Range (LRA):"), lraLabel_);

    mainLayout->addWidget(resultGroup);

    // --- Progress ---
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(false);
    mainLayout->addWidget(progressBar_);

    // --- Status ---
    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    mainLayout->addWidget(statusLabel_);

    mainLayout->addStretch();
}

void AnalysisPanel::showResult(const LoudnessResult& result)
{
    progressBar_->setVisible(false);
    statusLabel_->clear();

    integratedLabel_->setText(QString::asprintf("%+.1f LUFS", result.integratedLUFS));
    truePeakLabel_->setText(QString::asprintf("%+.1f dBTP", result.maxTruePeakDB));
    maxMomentaryLabel_->setText(QString::asprintf("%+.1f LUFS", result.maxMomentaryLUFS));
    maxShortTermLabel_->setText(QString::asprintf("%+.1f LUFS", result.maxShortTermLUFS));
    lraLabel_->setText(QString::asprintf("%.1f LU", result.loudnessRangeLU));
}

void AnalysisPanel::showProgress(int percent)
{
    progressBar_->setVisible(true);
    progressBar_->setValue(percent);
    statusLabel_->setText(QString::asprintf("Analyzing... %d%%", percent));
}

void AnalysisPanel::showError(const QString& msg)
{
    progressBar_->setVisible(false);
    statusLabel_->setStyleSheet(QStringLiteral("QLabel { color: red; }"));
    statusLabel_->setText(msg);
}

void AnalysisPanel::clear()
{
    integratedLabel_->setText(QStringLiteral("—"));
    truePeakLabel_->setText(QStringLiteral("—"));
    maxMomentaryLabel_->setText(QStringLiteral("—"));
    maxShortTermLabel_->setText(QStringLiteral("—"));
    lraLabel_->setText(QStringLiteral("—"));
    progressBar_->setVisible(false);
    statusLabel_->clear();
    statusLabel_->setStyleSheet({});
}
