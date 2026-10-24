#include "AnalysisWorker.hpp"

#include <QElapsedTimer>
#include <fstream>

AnalysisWorker::AnalysisWorker(QString path, humtrace::PipelineSettings settings, QObject* parent, bool forcePaged, humtrace::StreamingLimits limits)
    : QThread(parent), path_(std::move(path)), settings_(settings), streamLimits_(limits), forcePaged_(forcePaged) {}

AnalysisWorker::AnalysisWorker(QString path, humtrace::PipelineSettings settings, QString output,
    humtrace::StreamingLimits limits, bool replaceExisting, QObject* parent)
    : QThread(parent), path_(std::move(path)), settings_(settings), output_(std::move(output)),
      streamLimits_(limits), replaceExisting_(replaceExisting) {}

AnalysisWorker::AnalysisWorker(std::shared_ptr<humtrace::DiskAnalysis> storage, QString output,
    bool replaceExisting, bool fullPath, QObject* parent)
    : QThread(parent), output_(std::move(output)), replaceExisting_(replaceExisting),
      fullPath_(fullPath), storage_(std::move(storage)) {}

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
        if (storage_) {
            storage_->export_json(output_.toStdString(), replaceExisting_, fullPath_, callbacks);
            emit streamedReportReady(output_);
        } else if (!output_.isEmpty()) {
            (void)humtrace::stream_file_report(path_.toStdString(), output_.toStdString(), settings_,
                callbacks, streamLimits_, replaceExisting_);
            emit streamedReportReady(output_);
        } else {
            humtrace::validate_settings(settings_);
            std::ifstream input(humtrace::path_from_utf8(path_.toStdString()), std::ios::binary);
            if (!input) throw std::runtime_error("could not open input file");
            humtrace::WavStreamReader reader(input);
            const auto& metadata = reader.metadata();
            const auto window = humtrace::select_window_size(metadata, settings_);
            const auto hop = settings_.hop_size_samples ? settings_.hop_size_samples : window / 2;
            const auto windows = (metadata.frame_count - window) / hop + 2;
            const auto peakCost = 2 * (sizeof(humtrace::Track) + sizeof(humtrace::TrackObservation) +
                sizeof(humtrace::TrackPeak) + 4 * sizeof(std::size_t) + 64);
            const auto cost = static_cast<std::uint64_t>(metadata.channels) *
                (sizeof(humtrace::InterferenceFrame) + 20 * sizeof(humtrace::HarmonicMeasurement) +
                 settings_.spectrogram_rows * sizeof(float) + settings_.max_peaks_per_frame * peakCost +
                 sizeof(humtrace::TrackAmbiguity) + sizeof(humtrace::FramePeaks) + 128);
            const auto threshold = std::min<std::uint64_t>(16ULL * 1024 * 1024, settings_.max_result_bytes);
            const bool paged = forcePaged_ || windows > threshold / cost;
            auto result = std::make_shared<humtrace::AnalysisResult>();
            if (paged) {
                auto limits = streamLimits_;
                limits.max_resident_bytes = std::min(limits.max_resident_bytes, settings_.max_result_bytes);
                result->storage = humtrace::analyze_file_paged(path_.toStdString(), settings_, callbacks, limits);
                result->report = result->storage->report();
                result->views = result->storage->views();
            } else *result = humtrace::analyze_file(path_.toStdString(), settings_, callbacks, true);
            emit resultReady(std::move(result));
        }
    } catch (const humtrace::AnalysisCancelled&) {
        emit cancelledByUser();
    } catch (const std::exception& error) {
        emit failed(QString::fromUtf8(error.what()));
    } catch (...) {
        emit failed(QStringLiteral("unexpected non-standard exception"));
    }
}
