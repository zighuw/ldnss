#pragma once

#include "core/LoudnessResult.h"

#include <QObject>

#include <memory>

class AnalysisWorker;
class QThread;

class WorkerController : public QObject {
    Q_OBJECT

public:
    explicit WorkerController(QObject* parent = nullptr);
    ~WorkerController() override;

    WorkerController(const WorkerController&) = delete;
    WorkerController& operator=(const WorkerController&) = delete;

    void startAnalysis(const QString& path);
    void cancelAnalysis();

    bool isRunning() const;

signals:
    void analysisProgress(int percent);
    void analysisComplete(const LoudnessResult& result);
    void analysisError(const QString& msg);

private:
    QThread* thread_ = nullptr;
    AnalysisWorker* worker_ = nullptr;
    bool running_ = false;
};
