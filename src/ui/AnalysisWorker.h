#pragma once

#include "core/LoudnessResult.h"

#include <QObject>

#include <memory>

class SndfileDecoder;
class LoudnessAnalyzer;

class AnalysisWorker : public QObject {
    Q_OBJECT

public:
    explicit AnalysisWorker(QObject* parent = nullptr);
    ~AnalysisWorker() override;

public slots:
    void doAnalysis(const QString& path);

signals:
    void progressChanged(int percent);
    void analysisFinished(LoudnessResult result);
    void analysisError(const QString& msg);

private:
    std::unique_ptr<SndfileDecoder> decoder_;
    std::unique_ptr<LoudnessAnalyzer> analyzer_;
};
