#include "LoudnessChartWidget.h"

#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>

LoudnessChartWidget::LoudnessChartWidget(QWidget* parent)
    : QChartView(parent)
{
    auto* chart = new QChart;
    chart->setTitle(QStringLiteral("Loudness Over Time"));
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->legend()->setAlignment(Qt::AlignBottom);

    // X axis: seconds
    axisX_ = new QValueAxis;
    axisX_->setTitleText(QStringLiteral("Time (s)"));
    axisX_->setLabelFormat(QStringLiteral("%.0f"));
    chart->addAxis(axisX_, Qt::AlignBottom);

    // Y axis: LUFS
    axisY_ = new QValueAxis;
    axisY_->setTitleText(QStringLiteral("LUFS"));
    axisY_->setRange(-50.0, 10.0);
    axisY_->setLabelFormat(QStringLiteral("%.0f"));
    chart->addAxis(axisY_, Qt::AlignLeft);

    // Momentary (blue) — 400ms window
    momentarySeries_ = new QLineSeries;
    momentarySeries_->setName(QStringLiteral("Momentary"));
    momentarySeries_->setPen(QPen(QColor(0x21, 0x96, 0xF3), 1.0));  // Material Blue
    chart->addSeries(momentarySeries_);
    momentarySeries_->attachAxis(axisX_);
    momentarySeries_->attachAxis(axisY_);

    // Short-term (red) — 3s window
    shortTermSeries_ = new QLineSeries;
    shortTermSeries_->setName(QStringLiteral("Short-term"));
    shortTermSeries_->setPen(QPen(QColor(0xF4, 0x43, 0x36), 1.0));  // Material Red
    chart->addSeries(shortTermSeries_);
    shortTermSeries_->attachAxis(axisX_);
    shortTermSeries_->attachAxis(axisY_);

    setChart(chart);
    setRenderHint(QPainter::Antialiasing);
}

void LoudnessChartWidget::showResult(const LoudnessResult& result)
{
    // Populate momentary series
    QList<QPointF> momPoints;
    momPoints.reserve(static_cast<int>(result.momentaryHistory.size()));
    for (const auto& p : result.momentaryHistory)
        momPoints.append(QPointF(p.seconds, p.lufs));
    momentarySeries_->replace(momPoints);

    // Populate short-term series
    QList<QPointF> stPoints;
    stPoints.reserve(static_cast<int>(result.shortTermHistory.size()));
    for (const auto& p : result.shortTermHistory)
        stPoints.append(QPointF(p.seconds, p.lufs));
    shortTermSeries_->replace(stPoints);

    // Auto range X axis
    double maxSec = 0.0;
    if (!result.momentaryHistory.empty())
        maxSec = std::max(maxSec, result.momentaryHistory.back().seconds);
    if (!result.shortTermHistory.empty())
        maxSec = std::max(maxSec, result.shortTermHistory.back().seconds);
    axisX_->setRange(0.0, std::max(maxSec, 1.0));

    // Auto range Y axis, clamped to [-50, +10]
    double minLUFS = 0.0;
    double maxLUFS = 0.0;
    for (const auto& p : result.momentaryHistory) {
        minLUFS = std::min(minLUFS, p.lufs);
        maxLUFS = std::max(maxLUFS, p.lufs);
    }
    for (const auto& p : result.shortTermHistory) {
        minLUFS = std::min(minLUFS, p.lufs);
        maxLUFS = std::max(maxLUFS, p.lufs);
    }
    double lo = std::clamp(minLUFS - 2.0, -50.0, 10.0);
    double hi = std::clamp(maxLUFS + 1.0, -50.0, 10.0);
    axisY_->setRange(lo, hi);
}

void LoudnessChartWidget::clear()
{
    momentarySeries_->clear();
    shortTermSeries_->clear();
    axisX_->setRange(0.0, 1.0);
    axisY_->setRange(-50.0, 10.0);
}
