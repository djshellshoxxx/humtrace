#pragma once

#include "humtrace/session.hpp"

#include <QThread>
#include <atomic>
#include <memory>

// Runs one analysis job on its own thread. Owns the decoder, a settings snapshot and the
// cancellation flag; communicates only through queued signals carrying value data.
class AnalysisWorker : public QThread {
    Q_OBJECT
public:
    AnalysisWorker(QString path, humtrace::PipelineSettings settings, QObject* parent = nullptr);
    void requestCancel() { cancel_ = true; }

signals:
    void stageChanged(int stage, double fraction); // humtrace::Stage
    void finished(std::shared_ptr<humtrace::AnalysisResult> result);
    void cancelledByUser();
    void failed(QString message);

protected:
    void run() override;

private:
    QString path_;
    humtrace::PipelineSettings settings_;
    std::atomic<bool> cancel_{false};
};
