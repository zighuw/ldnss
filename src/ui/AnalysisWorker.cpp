#include "AnalysisWorker.h"
#include "core/LoudnessAnalyzer.h"
#include "core/SndfileDecoder.h"

#include <QThread>

#include <vector>

AnalysisWorker::AnalysisWorker(QObject* parent)
    : QObject(parent)
    , decoder_(new SndfileDecoder)
{
    qRegisterMetaType<LoudnessResult>("LoudnessResult");
}

AnalysisWorker::~AnalysisWorker() = default;

void AnalysisWorker::doAnalysis(const QString& path)
{
    if (!decoder_->open(path.toStdString())) {
        emit analysisError(QStringLiteral("Cannot open file: %1 (%2)")
                               .arg(path,
                                    QString::fromStdString(decoder_->lastError())));
        return;
    }

    const AudioFormat& fmt = decoder_->format();
    analyzer_.reset(new LoudnessAnalyzer(fmt.sampleRate, fmt.channels));

    static const int bufferFrames = 4096;
    std::vector<float> buffer(static_cast<size_t>(bufferFrames * fmt.channels));

    int lastPercent = -1;
    int64_t processed = 0;
    int64_t total = fmt.totalFrames;

    while (true) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            emit analysisError(QStringLiteral("Analysis cancelled"));
            return;
        }

        int64_t n = decoder_->readFrames(buffer.data(), bufferFrames);
        if (n <= 0)
            break;

        if (analyzer_->addFrames(buffer.data(), n) != 0) {
            emit analysisError(QStringLiteral("Analysis failed during processing"));
            return;
        }

        processed += n;
        int pct = (total > 0) ? static_cast<int>(processed * 100 / total) : 0;
        if (pct != lastPercent) {
            lastPercent = pct;
            emit progressChanged(pct);
        }
    }

    if (analyzer_->finalize() != 0) {
        emit analysisError(QStringLiteral("Finalize failed"));
        return;
    }

    emit analysisFinished(analyzer_->result());
}
