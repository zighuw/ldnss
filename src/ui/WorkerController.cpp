#include "WorkerController.h"
#include "AnalysisWorker.h"

#include <QThread>

WorkerController::WorkerController(QObject* parent)
    : QObject(parent)
    , thread_(new QThread(this))
    , worker_(new AnalysisWorker)
{
    worker_->moveToThread(thread_);

    // Forward worker signals to controller clients (queued by default).
    connect(worker_, &AnalysisWorker::progressChanged,
            this, &WorkerController::analysisProgress);
    connect(worker_, &AnalysisWorker::analysisFinished,
            this, [this](const LoudnessResult& r) {
                running_ = false;
                emit analysisComplete(r);
            });
    connect(worker_, &AnalysisWorker::analysisError,
            this, [this](const QString& msg) {
                running_ = false;
                emit analysisError(msg);
            });
}

WorkerController::~WorkerController()
{
    cancelAnalysis();
    delete worker_;
}

void WorkerController::startAnalysis(const QString& path)
{
    if (running_)
        return;

    running_ = true;
    thread_->start();

    QMetaObject::invokeMethod(worker_, "doAnalysis", Qt::QueuedConnection,
                              Q_ARG(QString, path));
}

void WorkerController::cancelAnalysis()
{
    if (!running_)
        return;

    thread_->requestInterruption();
    thread_->quit();
    thread_->wait();
    running_ = false;
}

bool WorkerController::isRunning() const
{
    return running_;
}
