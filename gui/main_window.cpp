#include "main_window.hpp"

#include "humtrace/version.hpp"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QThread>
#include <QVBoxLayout>
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
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("HumTrace %1").arg(humtrace::kVersion));
    auto* central = new QWidget;
    auto* root = new QVBoxLayout(central);

    auto* bar = new QHBoxLayout;
    openButton_ = new QPushButton(tr("&Open WAV…"));
    cancelButton_ = new QPushButton(tr("&Cancel"));
    exportButton_ = new QPushButton(tr("&Export JSON…"));
    channelBox_ = new QComboBox; channelBox_->setAccessibleName(tr("Channel"));
    for (auto* w : {static_cast<QWidget*>(openButton_), static_cast<QWidget*>(cancelButton_),
                    static_cast<QWidget*>(exportButton_)}) bar->addWidget(w);
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
    root->addWidget(progress_); root->addWidget(summary_);

    timeline_ = new TimelineWidget; spectrum_ = new SpectrumWidget;
    root->addWidget(timeline_); root->addWidget(spectrum_);

    table_ = new QTableWidget(0, 7);
    table_->setHorizontalHeaderLabels({tr("Time (s)"), tr("Peak (Hz)"), tr("Peak (dBFS)"),
                                       tr("50 Hz supported"), tr("60 Hz supported"),
                                       tr("Best 50 Hz prominence (dB)"), tr("Best 60 Hz prominence (dB)")});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setAccessibleName(tr("Findings table"));
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    root->addWidget(table_, 1);
    setCentralWidget(central);
    resize(980, 760);

    connect(openButton_, &QPushButton::clicked, this, &MainWindow::chooseFile);
    connect(cancelButton_, &QPushButton::clicked, this, &MainWindow::cancelAnalysis);
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
QString MainWindow::statusText() const { return summary_->text(); }

void MainWindow::setBusy(bool busy) {
    running_ = busy;
    openButton_->setEnabled(!busy);
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
    populateTable();
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

void MainWindow::selectFrame(int frame) {
    if (!report_) return;
    const int channel = channelBox_->currentIndex();
    const auto& frames = report_->channel_results[static_cast<std::size_t>(channel)].frames;
    if (frame < 0 || frame >= static_cast<int>(frames.size())) return;
    timeline_->setSelectedFrame(frame);
    if (table_->currentRow() != frame) table_->selectRow(frame);
    try {
        spectrum_->setSpectrum(humtrace::load_frame_spectrum(
            path_.toStdString(), report_->channel_results[static_cast<std::size_t>(channel)].channel_number,
            frames[static_cast<std::size_t>(frame)].start_sample, report_->frame_size_samples));
    } catch (const std::exception& e) {
        summary_->setText(tr("Could not load frame spectrum: %1").arg(e.what()));
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
