#include "main_window.hpp"

#include "humtrace/version.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFontDatabase>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSignalBlocker>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QThread>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
// Runs the core on a worker thread; reports back only via queued signals.
class Worker : public QObject {
    Q_OBJECT
public:
    Worker(QString path, humtrace::AnalysisOptions options)
        : path_(std::move(path)), options_(std::move(options)) {}
public slots:
    void run() {
        options_.progress = [this](double f) { emit progress(static_cast<int>(f * 100)); };
        try {
            auto report = std::make_unique<humtrace::AnalysisReport>(
                humtrace::analyze_wav_file(path_.toStdString(), options_));
            result.reset(report.release());
            emit done(true, QString());
        } catch (const humtrace::AnalysisCancelled&) {
            emit done(false, QStringLiteral("Analysis cancelled."));
        } catch (const std::exception& e) {
            emit done(false, QString::fromUtf8(e.what()));
        }
    }
signals:
    void progress(int percent);
    void done(bool ok, const QString& message);
public:
    std::unique_ptr<humtrace::AnalysisReport> result;
private:
    QString path_;
    humtrace::AnalysisOptions options_;
};

// Analyzes several files in turn (no display products) and summarizes them.
class CompareWorker : public QObject {
    Q_OBJECT
public:
    CompareWorker(QStringList paths, humtrace::AnalysisOptions options)
        : paths_(std::move(paths)), options_(std::move(options)) {}
public slots:
    void run() {
        try {
            const int total = static_cast<int>(paths_.size());
            for (int i = 0; i < total; ++i) {
                auto o = options_;
                o.progress = [this, i, total](double f) { emit progress(static_cast<int>((i + f) * 100 / total)); };
                auto report = humtrace::analyze_wav_file(paths_[i].toStdString(), o);
                result.push_back(humtrace::summarize(report));
            }
            emit done(true, QString());
        } catch (const humtrace::AnalysisCancelled&) {
            emit done(false, QStringLiteral("Comparison cancelled."));
        } catch (const std::exception& e) {
            emit done(false, QString::fromUtf8(e.what()));
        }
    }
signals:
    void progress(int percent);
    void done(bool ok, const QString& message);
public:
    std::vector<humtrace::FileSummary> result;
private:
    QStringList paths_;
    humtrace::AnalysisOptions options_;
};
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("HumTrace %1").arg(humtrace::kVersion));
    auto* central = new QWidget;
    auto* root = new QVBoxLayout(central);

    auto* bar = new QHBoxLayout;
    openButton_ = new QPushButton(tr("&Open WAV…"));
    cancelButton_ = new QPushButton(tr("&Cancel"));
    exportButton_ = new QPushButton(tr("&Export JSON…"));
    compareButton_ = new QPushButton(tr("Co&mpare files…"));
    compareButton_->setAccessibleDescription(tr("Analyze two or more WAV files and list tones they share"));
    channelBox_ = new QComboBox; channelBox_->setAccessibleName(tr("Channel"));
    for (auto* w : {static_cast<QWidget*>(openButton_), static_cast<QWidget*>(cancelButton_),
                    static_cast<QWidget*>(exportButton_), static_cast<QWidget*>(compareButton_)}) bar->addWidget(w);
    bar->addWidget(new QLabel(tr("Channel:"))); bar->addWidget(channelBox_); bar->addStretch();
    root->addLayout(bar);

    auto* settings = new QHBoxLayout;
    frameSizeBox_ = new QComboBox; frameSizeBox_->setAccessibleName(tr("Frame size"));
    frameSizeBox_->addItem(tr("Auto"), 0);
    for (int n = 1024; n <= 65536; n *= 2) frameSizeBox_->addItem(QString::number(n), n);
    thresholdSpin_ = new QDoubleSpinBox; thresholdSpin_->setRange(-140, 0); thresholdSpin_->setValue(-60);
    thresholdSpin_->setSuffix(tr(" dBFS")); thresholdSpin_->setAccessibleName(tr("Support threshold"));
    prominenceSpin_ = new QDoubleSpinBox; prominenceSpin_->setRange(0, 80); prominenceSpin_->setValue(10);
    prominenceSpin_->setSuffix(tr(" dB")); prominenceSpin_->setAccessibleName(tr("Minimum prominence"));
    settings->addWidget(new QLabel(tr("Frame size:"))); settings->addWidget(frameSizeBox_);
    settings->addWidget(new QLabel(tr("Threshold:"))); settings->addWidget(thresholdSpin_);
    settings->addWidget(new QLabel(tr("Prominence:"))); settings->addWidget(prominenceSpin_);
    settings->addStretch();
    root->addLayout(settings);

    progress_ = new QProgressBar; progress_->setRange(0, 100); progress_->setAccessibleName(tr("Analysis progress"));
    summary_ = new QLabel(tr("Open a WAV file to begin. Results are measurements, not source attribution."));
    summary_->setWordWrap(true); summary_->setAccessibleName(tr("Summary"));
    // Wrapped labels in a stretchy layout get squeezed below their text height.
    summary_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    summary_->setMinimumHeight(summary_->fontMetrics().lineSpacing() * 3 + 4);
    summary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(progress_); root->addWidget(summary_);

    timeline_ = new TimelineWidget; spectrum_ = new SpectrumWidget;
    waveform_ = new WaveformWidget; spectrogram_ = new SpectrogramWidget;
    viewTabs_ = new QTabWidget; viewTabs_->setAccessibleName(tr("Views"));
    viewTabs_->addTab(waveform_, tr("Waveform"));
    viewTabs_->addTab(spectrogram_, tr("Spectrogram"));
    viewTabs_->addTab(timeline_, tr("Timeline"));
    viewTabs_->addTab(spectrum_, tr("Spectrum"));
    viewTabs_->setCurrentIndex(1);
    root->addWidget(viewTabs_, 1);

    table_ = new QTableWidget(0, 7);
    table_->setHorizontalHeaderLabels({tr("Time (s)"), tr("Peak (Hz)"), tr("Peak (dBFS)"),
                                       tr("50 Hz supported"), tr("60 Hz supported"),
                                       tr("Best 50 Hz prominence (dB)"), tr("Best 60 Hz prominence (dB)")});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setAccessibleName(tr("Findings table"));
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tracksTable_ = new QTableWidget(0, 8);
    tracksTable_->setHorizontalHeaderLabels({tr("Frequency (Hz)"), tr("Start (s)"), tr("End (s)"), tr("Level (dBFS)"),
                                             tr("Prominence (dB)"), tr("Persistence"), tr("Drift (Hz/s)"),
                                             tr("Mains proximity (not a source claim)")});
    tracksTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tracksTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tracksTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    tracksTable_->setAccessibleName(tr("Tone tracks table"));
    tracksTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableTabs_ = new QTabWidget; tableTabs_->setAccessibleName(tr("Tables"));
    tableTabs_->addTab(table_, tr("Findings"));
    tableTabs_->addTab(tracksTable_, tr("Tone tracks"));
    root->addWidget(tableTabs_, 1);
    setCentralWidget(central);
    resize(980, 760);

    connect(openButton_, &QPushButton::clicked, this, &MainWindow::chooseFile);
    connect(cancelButton_, &QPushButton::clicked, this, &MainWindow::cancelAnalysis);
    connect(compareButton_, &QPushButton::clicked, this, &MainWindow::chooseCompareFiles);
    connect(spectrogram_, &SpectrogramWidget::frameClicked, this, &MainWindow::selectFrame);
    connect(tracksTable_, &QTableWidget::currentCellChanged, this, [this](int row) { if (row >= 0) selectTrackRow(row); });
    connect(exportButton_, &QPushButton::clicked, this, &MainWindow::exportReport);
    connect(channelBox_, &QComboBox::currentIndexChanged, this, &MainWindow::selectChannel);
    connect(timeline_, &TimelineWidget::frameClicked, this, &MainWindow::selectFrame);
    connect(table_, &QTableWidget::currentCellChanged, this, [this](int row) { if (row >= 0) selectFrame(row); });
    setBusy(false);
}

MainWindow::~MainWindow() {
    cancel_ = true;
    if (thread_) { thread_->quit(); thread_->wait(); }
}

int MainWindow::tableRowCount() const { return table_->rowCount(); }
int MainWindow::trackRowCount() const { return tracksTable_->rowCount(); }
QString MainWindow::statusText() const { return summary_->text(); }

void MainWindow::setBusy(bool busy) {
    running_ = busy;
    openButton_->setEnabled(!busy);
    compareButton_->setEnabled(!busy);
    cancelButton_->setEnabled(busy);
    exportButton_->setEnabled(!busy && report_);
    channelBox_->setEnabled(!busy && report_);
}

void MainWindow::chooseFile() {
    const auto path = QFileDialog::getOpenFileName(this, tr("Open WAV recording"), QString(),
                                                   tr("WAV audio (*.wav);;All files (*)"));
    if (!path.isEmpty()) openFile(path);
}

void MainWindow::openFile(const QString& path) {
    if (running_) return;
    path_ = path;
    cancel_ = false;
    humtrace::AnalysisOptions options;
    options.frame_size_samples = static_cast<std::size_t>(frameSizeBox_->currentData().toInt());
    options.support_threshold_dbfs = thresholdSpin_->value();
    options.minimum_prominence_db = prominenceSpin_->value();
    options.cancel = &cancel_;
    auto* worker = new Worker(path, options);
    auto* thread = new QThread(this);
    thread_ = thread;
    worker->moveToThread(thread);
    const auto generation = ++generation_;
    connect(thread, &QThread::started, worker, &Worker::run);
    connect(worker, &Worker::progress, this, &MainWindow::onProgress, Qt::QueuedConnection);
    connect(worker, &Worker::done, this, [this, worker, thread, generation](bool ok, const QString& message) {
        if (generation == generation_ && ok) report_ = std::move(worker->result);
        thread->quit();
        onDone(ok, message);
    }, Qt::QueuedConnection);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread] { if (thread_ == thread) thread_ = nullptr; });
    progress_->setValue(0);
    summary_->setText(tr("Analyzing %1…").arg(path));
    setBusy(true);
    thread_->start();
}

void MainWindow::cancelAnalysis() { cancel_ = true; }
void MainWindow::onProgress(int percent) { progress_->setValue(percent); }

void MainWindow::onDone(bool ok, const QString& message) {
    setBusy(false);
    if (!ok) {
        summary_->setText(tr("Analysis failed: %1").arg(message));
        emit analysisFinished(false);
        return;
    }
    progress_->setValue(100);
    channelBox_->blockSignals(true);
    channelBox_->clear();
    for (const auto& c : report_->channel_results) channelBox_->addItem(tr("Channel %1").arg(c.channel_number));
    channelBox_->blockSignals(false);
    QString text = tr("%1 — %2 Hz, %3 channel(s), %4 s, SHA-256 %5. Measurements only; they do not identify a source.")
        .arg(path_).arg(report_->sample_rate_hz).arg(report_->channels)
        .arg(report_->duration_seconds, 0, 'f', 2).arg(QString::fromStdString(report_->input_sha256));
    for (const auto& w : report_->warnings) text += tr(" Warning: %1.").arg(QString::fromStdString(w));
    summary_->setText(text);
    setBusy(false);
    selectChannel(0);
    emit analysisFinished(true);
}

void MainWindow::selectChannel(int index) {
    if (!report_ || index < 0) return;
    timeline_->setReport(report_.get(), index);
    waveform_->setReport(report_.get(), index);
    spectrogram_->setReport(report_.get(), index);
    populateTable();
    populateTracks();
    if (table_->rowCount() > 0) table_->selectRow(0);
}

void MainWindow::populateTable() {
    const auto& frames = report_->channel_results[static_cast<std::size_t>(channelBox_->currentIndex())].frames;
    table_->setRowCount(static_cast<int>(frames.size()));
    auto best = [](const humtrace::HarmonicCandidate& c) {
        double b = -1e9; bool any = false;
        for (const auto& h : c.harmonics) if (h.supports_threshold && std::isfinite(h.prominence_db)) { b = std::max(b, h.prominence_db); any = true; }
        return any ? QString::number(b, 'f', 1) : QStringLiteral("—");
    };
    for (int r = 0; r < static_cast<int>(frames.size()); ++r) {
        const auto& f = frames[static_cast<std::size_t>(r)];
        const QStringList cells = {
            QString::number(static_cast<double>(f.start_sample) / report_->sample_rate_hz, 'f', 3),
            QString::number(f.strongest_peak.frequency_hz, 'f', 2),
            std::isfinite(f.strongest_peak.level_dbfs) ? QString::number(f.strongest_peak.level_dbfs, 'f', 1) : QStringLiteral("silence"),
            QStringLiteral("%1/%2").arg(f.mains_50.supporting_harmonics).arg(f.mains_50.harmonics.size()),
            QStringLiteral("%1/%2").arg(f.mains_60.supporting_harmonics).arg(f.mains_60.harmonics.size()),
            best(f.mains_50), best(f.mains_60)};
        for (int c = 0; c < cells.size(); ++c) table_->setItem(r, c, new QTableWidgetItem(cells[c]));
    }
}

void MainWindow::populateTracks() {
    const auto& tracks = report_->channel_results[static_cast<std::size_t>(channelBox_->currentIndex())].tracks;
    const double rate = report_->sample_rate_hz ? report_->sample_rate_hz : 1.0;
    QSignalBlocker block(tracksTable_);
    tracksTable_->setRowCount(static_cast<int>(tracks.size()));
    for (int r = 0; r < static_cast<int>(tracks.size()); ++r) {
        const auto& t = tracks[static_cast<std::size_t>(r)];
        QString mains = tr("not near 50/60 Hz multiples");
        if (t.mains.nominal_hz > 0)
            mains = tr("near %1×%2 Hz (%3 Hz off; proximity only)").arg(t.mains.harmonic)
                        .arg(t.mains.nominal_hz, 0, 'f', 0).arg(t.mains.deviation_hz, 0, 'f', 2);
        const QStringList cells = {
            QString::number(t.median_frequency_hz, 'f', 2),
            QString::number(static_cast<double>(t.start_sample) / rate, 'f', 3),
            QString::number(static_cast<double>(t.end_sample) / rate, 'f', 3),
            QString::number(t.median_level_dbfs, 'f', 1),
            QString::number(t.median_prominence_db, 'f', 1),
            QString::number(t.persistence, 'f', 2),
            QString::number(t.drift_hz_per_second, 'f', 3),
            mains};
        for (int c = 0; c < cells.size(); ++c) tracksTable_->setItem(r, c, new QTableWidgetItem(cells[c]));
    }
}

void MainWindow::selectTrackRow(int row) {
    if (!report_ || channelBox_->currentIndex() < 0) return;
    const auto& ch = report_->channel_results[static_cast<std::size_t>(channelBox_->currentIndex())];
    if (row < 0 || row >= static_cast<int>(ch.tracks.size())) return;
    if (tracksTable_->currentRow() != row) tracksTable_->selectRow(row);
    const auto start = ch.tracks[static_cast<std::size_t>(row)].start_sample;
    const auto it = std::lower_bound(ch.frames.begin(), ch.frames.end(), start,
        [](const humtrace::InterferenceFrame& f, std::uint64_t s) { return f.start_sample < s; });
    if (it != ch.frames.end()) selectFrame(static_cast<int>(it - ch.frames.begin()));
}

void MainWindow::selectFrame(int frame) {
    if (!report_) return;
    const int channel = channelBox_->currentIndex();
    const auto& frames = report_->channel_results[static_cast<std::size_t>(channel)].frames;
    if (frame < 0 || frame >= static_cast<int>(frames.size())) return;
    timeline_->setSelectedFrame(frame);
    waveform_->setSelectedFrame(frame);
    spectrogram_->setSelectedFrame(frame);
    if (table_->currentRow() != frame) table_->selectRow(frame);
    try {
        spectrum_->setSpectrum(humtrace::load_frame_spectrum(
            path_.toStdString(), report_->channel_results[static_cast<std::size_t>(channel)].channel_number,
            frames[static_cast<std::size_t>(frame)].start_sample, report_->frame_size_samples));
    } catch (const std::exception& e) {
        summary_->setText(tr("Could not load frame spectrum: %1").arg(e.what()));
    }
}

void MainWindow::chooseCompareFiles() {
    const auto paths = QFileDialog::getOpenFileNames(this, tr("Select two or more WAV recordings to compare"), QString(),
                                                     tr("WAV audio (*.wav);;All files (*)"));
    if (paths.isEmpty()) return;
    if (paths.size() < 2) { summary_->setText(tr("Comparison needs at least two files.")); return; }
    showCompareDialog_ = true;
    compareFiles(paths);
}

void MainWindow::compareFiles(const QStringList& paths) {
    if (running_) return;
    if (paths.size() < 2) {
        comparisonText_.clear();
        summary_->setText(tr("Comparison needs at least two files."));
        emit compareFinished(false);
        return;
    }
    cancel_ = false;
    comparing_ = true;
    comparePaths_ = paths;
    humtrace::AnalysisOptions options;
    options.frame_size_samples = static_cast<std::size_t>(frameSizeBox_->currentData().toInt());
    options.support_threshold_dbfs = thresholdSpin_->value();
    options.minimum_prominence_db = prominenceSpin_->value();
    options.build_display = false;
    options.cancel = &cancel_;
    auto* worker = new CompareWorker(paths, options);
    auto* thread = new QThread(this);
    thread_ = thread;
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &CompareWorker::run);
    connect(worker, &CompareWorker::progress, this, &MainWindow::onProgress, Qt::QueuedConnection);
    connect(worker, &CompareWorker::done, this, [this, worker, thread](bool ok, const QString& message) {
        if (ok) comparison_ = std::move(worker->result);
        thread->quit();
        onCompareDone(ok, message);
    }, Qt::QueuedConnection);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(thread, &QThread::finished, this, [this, thread] { if (thread_ == thread) thread_ = nullptr; });
    progress_->setValue(0);
    summary_->setText(tr("Comparing %1 files…").arg(paths.size()));
    setBusy(true);
    thread_->start();
}

void MainWindow::onCompareDone(bool ok, const QString& message) {
    setBusy(false);
    comparing_ = false;
    const bool show = showCompareDialog_;
    showCompareDialog_ = false;
    if (!ok) {
        comparisonText_.clear();
        summary_->setText(tr("Comparison failed: %1").arg(message));
        emit compareFinished(false);
        return;
    }
    progress_->setValue(100);
    comparisonText_ = QString::fromStdString(humtrace::format_comparison(comparison_));
    summary_->setText(tr("Compared %1 files. Shared tones are measurements, not source attribution.").arg(comparison_.size()));
    if (show) showComparisonDialog();
    emit compareFinished(true);
}

void MainWindow::showComparisonDialog() {
    auto* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("File comparison"));
    auto* layout = new QVBoxLayout(dlg);
    auto* text = new QPlainTextEdit(comparisonText_);
    text->setReadOnly(true);
    text->setLineWrapMode(QPlainTextEdit::NoWrap);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    text->setAccessibleName(tr("Comparison result"));
    layout->addWidget(text);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto* exportBtn = buttons->addButton(tr("Export JSON…"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    connect(exportBtn, &QPushButton::clicked, dlg, [this, dlg] {
        const auto path = QFileDialog::getSaveFileName(dlg, tr("Export comparison JSON"),
                                                       QStringLiteral("humtrace-comparison.json"), tr("JSON (*.json)"));
        if (!path.isEmpty() && exportComparisonJson(path)) summary_->setText(tr("Comparison saved: %1").arg(path));
    });
    dlg->resize(900, 520);
    dlg->open();   // non-blocking
}

bool MainWindow::exportComparisonJson(const QString& path) {
    if (comparison_.empty()) return false;
    const QString target = QFileInfo(path).absoluteFilePath();
    for (const auto& input : comparePaths_) {
        const QFileInfo in(input), out(path);
        if (target == in.absoluteFilePath() || (out.exists() && in.canonicalFilePath() == out.canonicalFilePath())) {
            summary_->setText(tr("Export refused: output path equals input file %1").arg(input));
            return false;
        }
    }
    try {
        humtrace::write_file_safely(path.toStdString(), humtrace::serialize_comparison_json(comparison_),
                                    comparePaths_.first().toStdString());
        return true;
    } catch (const std::exception& e) {
        summary_->setText(tr("Export failed: %1").arg(e.what()));
        return false;
    }
}

bool MainWindow::exportJson(const QString& path) {
    if (!report_) return false;
    try {
        humtrace::write_file_safely(path.toStdString(), humtrace::serialize_report_json(*report_), path_.toStdString());
        return true;
    } catch (const std::exception& e) {
        summary_->setText(tr("Export failed: %1").arg(e.what()));
        return false;
    }
}

void MainWindow::exportReport() {
    const auto path = QFileDialog::getSaveFileName(this, tr("Export JSON report"), QStringLiteral("humtrace-report.json"),
                                                   tr("JSON (*.json)"));
    if (!path.isEmpty() && exportJson(path)) summary_->setText(tr("Report saved: %1 (any previous file kept as .bak)").arg(path));
}

#include "main_window.moc"
