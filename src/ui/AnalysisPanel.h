#pragma once

#include "core/LoudnessResult.h"

#include <QWidget>

class QLabel;
class QProgressBar;

class AnalysisPanel : public QWidget {
    Q_OBJECT

public:
    explicit AnalysisPanel(QWidget* parent = nullptr);

public slots:
    void showResult(const LoudnessResult& result);
    void showProgress(int percent);
    void showError(const QString& msg);
    void clear();

private:
    QLabel* integratedLabel_;
    QLabel* truePeakLabel_;
    QLabel* maxMomentaryLabel_;
    QLabel* maxShortTermLabel_;
    QLabel* lraLabel_;
    QLabel* statusLabel_;
    QProgressBar* progressBar_;
};
