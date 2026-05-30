#pragma once

#include "core/LoudnessResult.h"

#include <QChartView>

class QLineSeries;
class QValueAxis;

class LoudnessChartWidget : public QChartView {
    Q_OBJECT

public:
    explicit LoudnessChartWidget(QWidget* parent = nullptr);

public slots:
    void showResult(const LoudnessResult& result);
    void clear();

private:
    QLineSeries* momentarySeries_;
    QLineSeries* shortTermSeries_;
    QValueAxis* axisX_;
    QValueAxis* axisY_;
};
