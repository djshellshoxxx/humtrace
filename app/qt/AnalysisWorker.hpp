#pragma once

#include "humtrace/session.hpp"
#include "humtrace/streaming.hpp"

#include <QThread>
#include <atomic>
#include <memory>

// Runs one analysis job on its own thread. Owns the decoder, a settings snapshot and the
// cancellation flag; communicates only through queued signals carrying value data.
class AnalysisWorker : public QThread {
    Q_OBJECT
public:
    AnalysisWorker(QString path, humtrace::PipelineSettings settings, QObject* parent = nullptr, bool forcePaged = false, humtrace::StreamingLimits limits = {});
    AnalysisWorker(QString path, humtrace::PipelineSettings settings, QString output,
                   humtrace::StreamingLimits limits, bool replaceExisting, QObject* parent = nullptr);
    AnalysisWorker(std::shared_ptr<humtrace::DiskAnalysis> storage, QString output,
                   bool replaceExisting, bool fullPath, QObject* parent = nullptr);
    void requestCancel() { cancel_ = true; }

signals:
    void stageChanged(int stage, double fraction); // humtrace::Stage
    void resultReady(std::shared_ptr<humtrace::AnalysisResult> result);
    void streamedReportReady(QString path);
    void cancelledByUser();
    void failed(QString message);

protected:
    void run() override;

private:
    QString path_;
    humtrace::PipelineSettings settings_;
    QString output_;
    humtrace::StreamingLimits streamLimits_;
    bool replaceExisting_{false};
    bool fullPath_{false};
    bool forcePaged_{false};
    std::shared_ptr<humtrace::DiskAnalysis> storage_;
    std::atomic<bool> cancel_{false};
};
