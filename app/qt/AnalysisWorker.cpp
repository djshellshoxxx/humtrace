#include "AnalysisWorker.hpp"

#include <QElapsedTimer>

AnalysisWorker::AnalysisWorker(QString path, humtrace::PipelineSettings settings, QObject* parent)
    : QThread(parent), path_(std::move(path)), settings_(settings) {}

void AnalysisWorker::run() {
    try {
        QElapsedTimer throttle;
        throttle.start();
        humtrace::SessionCallbacks callbacks;
        int lastStage = -1;
        callbacks.progress = [&](humtrace::Stage stage, double fraction) {
            const int s = static_cast<int>(stage);
            // Throttle to ~10 updates/s but always deliver stage changes and completion.
            if (s != lastStage || fraction >= 1.0 || throttle.elapsed() >= 100) {
                lastStage = s;
                throttle.restart();
                emit stageChanged(s, fraction);
            }
        };
        callbacks.cancelled = [this] { return cancel_.load(); };
        auto result = std::make_shared<humtrace::AnalysisResult>(
            humtrace::analyze_file(path_.toStdString(), settings_, callbacks, true));
        emit finished(std::move(result));
    } catch (const humtrace::AnalysisCancelled&) {
        emit cancelledByUser();
    } catch (const std::exception& error) {
        emit failed(QString::fromUtf8(error.what()));
    }
}
