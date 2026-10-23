#include "MainWindow.hpp"
#include "humtrace/music.hpp"

#include "AnalysisWorker.hpp"
#include "PagedData.hpp"
#include "ComparisonDialog.hpp"
#include "FindingsModel.hpp"
#include "SpectrogramChart.hpp"
#include "SpectrumChart.hpp"
#include "Theme.hpp"
#include "ViewState.hpp"
#include "WaveformChart.hpp"
#include "humtrace/compare.hpp"
#include "humtrace/wav.hpp"
#include "humtrace/version.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QEventLoop>
#include <QPushButton>
#include <cmath>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QProgressDialog>
#include <QSettings>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <fstream>
#include <tuple>

using humtrace::ui::JobState;

namespace {
bool sameSettings(const humtrace::PipelineSettings& a, const humtrace::PipelineSettings& b) {
    const auto values = [](const humtrace::PipelineSettings& s) {
        return std::tie(s.max_interleaved_window_samples, s.max_result_bytes,
            s.tracker.fixed_hz_tolerance, s.tracker.relative_tolerance, s.tracker.bin_spacing_hz,
            s.tracker.bin_fraction, s.tracker.max_gap_frames, s.tracker.min_observed_frames,
            s.welch.segment_size, s.welch.hop_size, s.welch.remove_mean,
            s.peak_min_level_dbfs, s.peak_min_prominence_db, s.max_peaks_per_frame,
            s.support_threshold_dbfs, s.minimum_prominence_db, s.waveform_buckets,
            s.spectrogram_rows, s.spectrogram_min_hz, s.spectrogram_floor_dbfs,
            s.frame_size_samples, s.hop_size_samples, s.spectrum_remove_mean,
            s.peak_search.minimum_frequency_hz, s.peak_search.maximum_frequency_hz,
            s.peak_search.minimum_separation_hz, s.peak_search.floor_guard_bins,
            s.peak_search.floor_radius_bins, s.harmonic_tolerance_hz);
    };
    return values(a) == values(b);
}

QString stageText(int stage) {
    switch (static_cast<humtrace::Stage>(stage)) {
    case humtrace::Stage::Hashing: return QObject::tr("Hashing input");
    case humtrace::Stage::Analyzing: return QObject::tr("Analyzing");
    case humtrace::Stage::Finalizing: return QObject::tr("Finalizing");
    case humtrace::Stage::Done: return QObject::tr("Complete");
    }
    return {};
}

QString spectrumMethod(const humtrace::AnalysisReport& r) {
    return QObject::tr("Spectrum: Hann window, %1 samples (%2 s), FFT length %1, bin spacing %3 Hz, peak-amplitude dBFS, frame mean %4. "
                       "Display rows show floor-clamped maximum FFT bins. Frequency uncertainty is not estimated; bin spacing is not an error bound.")
        .arg(QString::number(r.frame_size_samples), QString::number(double(r.frame_size_samples) / r.sample_rate_hz, 'f', 3),
             QString::number(double(r.sample_rate_hz) / double(r.frame_size_samples), 'f', 3),
             r.spectrum_remove_mean ? QObject::tr("removed") : QObject::tr("retained"));
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), view_(new ViewState(this)) {
    setWindowTitle(tr("HumTrace"));
    setAcceptDrops(true);
    buildUi();
    buildActions();
    loadRecent();
    setState(JobState::Empty);
    resize(1440, 900);
}

MainWindow::~MainWindow() {
    if (worker_) {
        worker_->requestCancel();
        worker_->wait();
    }
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(6, 4, 6, 4);

    header_ = new QLabel(tr("No recording open. Use Open Recording (Ctrl+O) or drop a WAV file here. Supported: PCM 8/16/24/32-bit and IEEE float 32/64-bit WAV. Files are opened read-only."));
    header_->setTextFormat(Qt::PlainText);
    header_->setWordWrap(true);
    header_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // never force the window wider
    header_->setAccessibleName(tr("Recording identity"));
    header_->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    summary_ = new QLabel;
    summary_->setTextFormat(Qt::PlainText);
    summary_->setWordWrap(true);
    summary_->setAccessibleName(tr("Analysis summary"));
    layout->addWidget(header_);
    layout->addWidget(summary_);

    auto* row = new QWidget;
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    scaleBox_ = new QComboBox;
    scaleBox_->addItems({tr("Log frequency"), tr("Linear frequency")});
    scaleBox_->setAccessibleName(tr("Spectrogram frequency scale"));
    paletteBox_ = new QComboBox;
    paletteBox_->addItems({tr("Default palette"), tr("Grayscale palette")});
    paletteBox_->setAccessibleName(tr("Spectrogram palette"));
    floorBox_ = new QDoubleSpinBox;
    floorBox_->setRange(-160, -20);
    floorBox_->setValue(-100);
    floorBox_->setSuffix(" dBFS");
    floorBox_->setAccessibleName(tr("Spectrogram floor"));
    ceilingBox_ = new QDoubleSpinBox;
    ceilingBox_->setRange(-100, 0);
    ceilingBox_->setValue(-10);
    ceilingBox_->setSuffix(" dBFS");
    ceilingBox_->setAccessibleName(tr("Spectrogram ceiling"));
    auto* note = new QLabel(tr("Display only; does not change analysis"));
    for (QWidget* w : std::initializer_list<QWidget*>{scaleBox_, paletteBox_, floorBox_, ceilingBox_, note}) rl->addWidget(w);
    rl->addStretch();
    layout->addWidget(row);
    for (QWidget* w : std::initializer_list<QWidget*>{scaleBox_, paletteBox_})
        connect(qobject_cast<QComboBox*>(w), &QComboBox::currentIndexChanged, this, &MainWindow::applyDisplayOptions);
    displayTimer_ = new QTimer(this);
    displayTimer_->setSingleShot(true);
    displayTimer_->setInterval(150);
    connect(displayTimer_, &QTimer::timeout, this, &MainWindow::applyDisplayOptionsNow);
    connect(floorBox_, &QDoubleSpinBox::valueChanged, this, &MainWindow::applyDisplayOptions);
    connect(ceilingBox_, &QDoubleSpinBox::valueChanged, this, &MainWindow::applyDisplayOptions);

    waveform_ = new WaveformChart(view_);
    spectrogram_ = new SpectrogramChart(view_);
    connect(spectrogram_, &SpectrogramChart::readFailed, this, &MainWindow::dataReadFailed, Qt::QueuedConnection);
    spectrum_ = new SpectrumChart;
    frameTable_ = new QTableWidget(0, 3);
    frameTable_->setHorizontalHeaderLabels({tr("Measurement"), tr("Frequency or range (Hz)"), tr("Level (dBFS)")});
    frameTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    frameTable_->horizontalHeader()->setStretchLastSection(true);
    frameTable_->setAccessibleName(tr("Spectrum measurement table for the frame at the cursor"));
    frameTable_->setAccessibleDescription(tr("Strongest peak, harmonic measurements and every plotted display row. Display rows are maximum FFT bin levels with a stored floor; row width is not frequency uncertainty."));
    auto* tabs = new QTabWidget;
    tabs->addTab(spectrum_, tr("Spectrum"));
    tabs->addTab(frameTable_, tr("Spectrum table"));
    tabs->setAccessibleName(tr("Selected-frame spectrum views"));
    layout->addWidget(waveform_, 2);
    layout->addWidget(spectrogram_, 4);
    spectrumInfo_ = new QLabel;
    spectrumInfo_->setTextFormat(Qt::PlainText);
    spectrumInfo_->setWordWrap(true);
    spectrumInfo_->setAccessibleName(tr("Spectrum method and resolution"));
    layout->addWidget(spectrumInfo_);
    layout->addWidget(tabs, 3);
    setCentralWidget(central);

    // Left rail: channels and recent files.
    auto* left = new QDockWidget(tr("Recording"), this);
    left->setObjectName("leftRail");
    auto* lw = new QWidget;
    auto* ll = new QVBoxLayout(lw);
    channels_ = new QListWidget;
    channels_->setAccessibleName(tr("Channels"));
    recent_ = new QListWidget;
    recent_->setAccessibleName(tr("Recent recordings"));
    auto* clear = new QPushButton(tr("Clear recent list"));
    ll->addWidget(new QLabel(tr("Channels (analyzed independently)")));
    ll->addWidget(channels_, 1);
    ll->addWidget(new QLabel(tr("Recent recordings")));
    ll->addWidget(recent_, 2);
    ll->addWidget(clear);
    left->setWidget(lw);
    addDockWidget(Qt::LeftDockWidgetArea, left);
    left->setMinimumWidth(200);
    connect(channels_, &QListWidget::currentRowChanged, this, [this](int row) { if (row >= 0) view_->setChannel(row); });
    connect(view_, &ViewState::channelChanged, this, [this] {
        if (channels_->currentRow() != view_->channel()) channels_->setCurrentRow(view_->channel());
        if (paged_) loadFindingPage(0);
        updateFrameViews();
    });
    connect(recent_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) { openRecording(item->data(Qt::UserRole).toString()); });
    connect(clear, &QPushButton::clicked, this, [this] {
        QSettings().remove("recent");
        recent_->clear();
    });

    // Right: findings and inspector.
    auto* right = new QDockWidget(tr("Findings"), this);
    right->setObjectName("rightRail");
    auto* rw = new QWidget;
    auto* rl2 = new QVBoxLayout(rw);
    model_ = new FindingsModel(this);
    proxy_ = new QSortFilterProxyModel(this);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(Qt::UserRole);
    table_ = new QTableView;
    table_->setModel(proxy_);
    table_->setSortingEnabled(true);
    table_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder); // default: source order, never a hidden score
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setAccessibleName(tr("Findings list"));
    inspector_ = new QTextBrowser;
    inspector_->setAccessibleName(tr("Finding inspector"));
    inspector_->setPlainText(tr("Select a finding to see measurements, evidence, settings and limitations."));
    inspector_->setFont(QFont("monospace"));
    findingPageLabel_ = new QLabel;
    findingPageLabel_->setWordWrap(true);
    findingPageLabel_->setAccessibleName(tr("Finding page and total count"));
    prevPage_ = new QPushButton(tr("Previous tracks"));
    nextPage_ = new QPushButton(tr("Next tracks"));
    connect(prevPage_, &QPushButton::clicked, this, [this] { if (findingPage_ >= 256) { loadFindingPage(findingPage_ - 256); if (model_->rowCount()) table_->selectRow(0); } });
    connect(nextPage_, &QPushButton::clicked, this, [this] { loadFindingPage(findingPage_ + 256); if (model_->rowCount()) table_->selectRow(0); });
    auto* pageControls = new QHBoxLayout;
    pageControls->addWidget(prevPage_); pageControls->addWidget(nextPage_);
    rl2->addWidget(findingPageLabel_);
    rl2->addLayout(pageControls);
    rl2->addWidget(table_, 2);
    rl2->addWidget(inspector_, 3);
    right->setWidget(rw);
    addDockWidget(Qt::RightDockWidgetArea, right);
    right->setMinimumWidth(340);
    connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &MainWindow::onFindingSelected);
    connect(view_, &ViewState::cursorChanged, this, &MainWindow::onCursorChanged);

    stateLabel_ = new QLabel;
    stateLabel_->setAccessibleName(tr("Job state"));
    progress_ = new QProgressBar;
    progress_->setMaximumWidth(260);
    progress_->setAccessibleName(tr("Progress"));
    statusBar()->addWidget(stateLabel_, 1);
    statusBar()->addPermanentWidget(progress_);
    progress_->hide();

    setTabOrder(waveform_, spectrogram_);
}

void MainWindow::buildActions() {
    auto* tb = addToolBar(tr("Main"));
    tb->setObjectName("mainToolbar");
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto make = [&](const QString& text, const QKeySequence& key, const QString& tip, auto slot) {
        auto* a = new QAction(text, this);
        a->setShortcut(key);
        a->setToolTip(tip + " (" + key.toString(QKeySequence::NativeText) + ")");
        a->setStatusTip(tip);
        connect(a, &QAction::triggered, this, slot);
        tb->addAction(a);
        return a;
    };
    openAct_ = make(tr("Open Recording"), QKeySequence::Open, tr("Open a WAV recording (read-only)"), &MainWindow::chooseAndOpen);
    analyzeAct_ = make(tr("Analyze"), QKeySequence("Ctrl+R"), tr("Run analysis"), [this] { startAnalysis(); });
    cancelAct_ = make(tr("Cancel"), QKeySequence("Ctrl+."), tr("Cancel the running analysis"), &MainWindow::cancelAnalysis);
    compareAct_ = make(tr("Compare"), QKeySequence("Ctrl+Shift+C"), tr("Compare with a second recording"), &MainWindow::chooseAndCompare);
    exportAct_ = make(tr("Export"), QKeySequence("Ctrl+E"), tr("Export a JSON report"), &MainWindow::chooseAndExport);
    settingsAct_ = make(tr("Settings"), QKeySequence("Ctrl+,"), tr("Analysis settings"), &MainWindow::showSettings);
    nextAct_ = new QAction(tr("Next Finding"), this);
    nextAct_->setShortcut(QKeySequence("Ctrl+Down"));
    prevAct_ = new QAction(tr("Previous Finding"), this);
    prevAct_->setShortcut(QKeySequence("Ctrl+Up"));
    connect(nextAct_, &QAction::triggered, this, [this] { stepFinding(1); });
    connect(prevAct_, &QAction::triggered, this, [this] { stepFinding(-1); });
    contrastAct_ = new QAction(tr("High-contrast theme"), this);
    contrastAct_->setCheckable(true);
    connect(contrastAct_, &QAction::toggled, this, [this](bool on) {
        theme::apply(*qApp, on);
        for (auto* chart : findChildren<QWidget*>()) chart->update();
    });
    auto* help = new QAction(tr("Keyboard help and limitations"), this);
    help->setShortcut(QKeySequence::HelpContents);
    connect(help, &QAction::triggered, this, &MainWindow::showHelp);

    auto* file = menuBar()->addMenu(tr("&File"));
    streamExportAct_ = new QAction(tr("Export long-file report"), this);
    streamExportAct_->setShortcut(QKeySequence("Ctrl+Shift+E"));
    streamExportAct_->setToolTip(tr("Analyze and export with bounded memory and temporary disk space"));
    connect(streamExportAct_, &QAction::triggered, this, &MainWindow::chooseAndStreamExport);
    file->addActions({openAct_, exportAct_, streamExportAct_});
    file->addAction(tr("Quit"), QKeySequence::Quit, this, &QWidget::close);
    auto* analysis = menuBar()->addMenu(tr("&Analysis"));
    analysis->addActions({analyzeAct_, cancelAct_, compareAct_, settingsAct_, nextAct_, prevAct_});
    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(contrastAct_);
    menuBar()->addMenu(tr("&Help"))->addAction(help);
}

void MainWindow::setState(JobState next) {
    if (!humtrace::ui::can_transition(state_, next) && state_ != next)
        qWarning("HumTrace: unexpected job transition %d -> %d", int(state_), int(next));
    state_ = next;
    stateLabel_->setText(QString::fromUtf8(humtrace::ui::label(next).data(), int(humtrace::ui::label(next).size())));
    stateLabel_->setAccessibleDescription(stateLabel_->text());
    updateActions();
}

void MainWindow::updateActions() {
    using humtrace::ui::is_active;
    const bool active = is_active(state_);
    const bool haveFile = !path_.isEmpty();
    openAct_->setEnabled(!active);
    analyzeAct_->setEnabled(haveFile && !active);
    cancelAct_->setEnabled(active && state_ != JobState::Cancelling);
    compareAct_->setEnabled(result_ && !active && state_ == JobState::Complete);
    exportAct_->setEnabled(result_ && !active && state_ == JobState::Complete);
    streamExportAct_->setEnabled(haveFile && !active && !compareBusy_);
    settingsAct_->setEnabled(!active);
    nextAct_->setEnabled(model_ && model_->rowCount() > 0);
    prevAct_->setEnabled(model_ && model_->rowCount() > 0);
    progress_->setVisible(active);
    findingPageLabel_->setVisible(bool(paged_));
    prevPage_->setVisible(bool(paged_)); nextPage_->setVisible(bool(paged_));
}

void MainWindow::showError(const QString& stage, const QString& message) {
    if (errorHandler_) { errorHandler_(stage, message); return; }
    QMessageBox::warning(this, tr("HumTrace"),
        tr("%1 failed for \"%2\".\n\n%3\n\nThe original file was not modified. Check that it is a supported WAV file and try again.")
            .arg(stage, QFileInfo(path_).fileName(), message));
}

void MainWindow::clearResults() {
    spectrogram_->setData(nullptr, nullptr, nullptr);
    paged_.reset();
    result_.reset();
    selectedRow_.clear();
    findings_.clear();
    model_->setRows({});
    waveform_->setViews(nullptr, 0);
    spectrogram_->setData(nullptr, nullptr, nullptr);
    spectrum_->setFrame(nullptr, nullptr, 0, -100, 0);
    frameTable_->setRowCount(0);
    spectrumInfo_->clear();
    spectrumInfo_->setAccessibleDescription({});
    summary_->clear();
    summary_->setAccessibleDescription({});
    inspector_->setPlainText(tr("Select a finding to see measurements, evidence, settings and limitations."));
    view_->reset(0.0, 1.0);
}

void MainWindow::openRecording(const QString& path) {
    if (humtrace::ui::is_active(state_) || compareBusy_) {
        statusBar()->showMessage(tr("Busy: finish or cancel the current job before opening another recording."), 5000);
        return;
    }
    // Validate first: a bad or missing file must not discard the current results.
    humtrace::WavMetadata m;
    try {
        std::ifstream in(humtrace::path_from_utf8(path.toStdString()), std::ios::binary);
        if (!in) throw std::runtime_error("could not open input file");
        humtrace::WavStreamReader reader(in);
        m = reader.metadata();
    } catch (const std::exception& e) {
        const auto previous = path_;
        path_ = path; // so the error names the file the user tried to open
        showError(tr("Opening"), QString::fromUtf8(e.what()));
        path_ = previous;
        return;
    }
    path_.clear();
    clearResults();
    setState(JobState::Opening);
    path_ = path;
    header_->setToolTip(path);
    header_->setText(tr("%1 | %2 s | %3 Hz | %4 channel(s) | %5-bit %6 | SHA-256 computed during analysis")
        .arg(header_->fontMetrics().elidedText(QFileInfo(path).fileName(), Qt::ElideMiddle, 420), QString::number(double(m.frame_count) / m.sample_rate_hz, 'f', 2),
             QString::number(m.sample_rate_hz), QString::number(m.channels), QString::number(m.bit_depth),
             m.format_code == 3 ? tr("float") : tr("PCM")));
    header_->setAccessibleDescription(header_->text());
    channels_->clear();
    for (int c = 1; c <= m.channels; ++c) channels_->addItem(tr("Channel %1").arg(c));
    channels_->setCurrentRow(0);
    addRecent(path);
    setState(JobState::Ready);
}

void MainWindow::startAnalysis() {
    if (path_.isEmpty() || humtrace::ui::is_active(state_) || compareBusy_) return;
    startWorker(new AnalysisWorker(path_, settings_, this, forcePaged_, streamLimits_));
}

void MainWindow::startStreamingExport(const QString& output, bool replaceExisting) {
    if (path_.isEmpty() || output.isEmpty() || humtrace::ui::is_active(state_) || compareBusy_) return;
    startWorker(new AnalysisWorker(path_, settings_, output, streamLimits_, replaceExisting, this));
}

void MainWindow::startWorker(AnalysisWorker* worker, bool preserveResults) {
    restoreChannel_ = std::max(0, channels_->currentRow());
    if (!preserveResults) clearResults();
    setState(preserveResults ? JobState::Finalizing : JobState::Hashing);
    progress_->setRange(0, 1000);
    progress_->setValue(0);
    worker_ = worker;
    const auto generation = ++jobGeneration_;
    connect(worker_, &AnalysisWorker::stageChanged, this, [this, generation](int stage, double fraction) {
        if (generation != jobGeneration_) return;
        const auto next = static_cast<humtrace::Stage>(stage) == humtrace::Stage::Hashing ? JobState::Hashing
                          : static_cast<humtrace::Stage>(stage) == humtrace::Stage::Analyzing ? JobState::Analyzing
                                                                                               : JobState::Finalizing;
        if (state_ == JobState::Cancelling) return; // keep showing "Cancelling" until the job stops
        if (state_ != next) setState(next);
        progress_->setValue(int(fraction * 1000));
        stateLabel_->setText(tr("%1: %2%").arg(stageText(stage)).arg(int(fraction * 100)));
        stateLabel_->setAccessibleDescription(stateLabel_->text());
    });
    connect(worker_, &AnalysisWorker::resultReady, this, [this, generation](std::shared_ptr<humtrace::AnalysisResult> result) { if (generation == jobGeneration_) onFinished(std::move(result)); });
    connect(worker_, &AnalysisWorker::streamedReportReady, this, [this, generation](const QString& output) {
        if (generation != jobGeneration_) return;
        releaseWorker();
        // Publication is the completion boundary. A late cancel must not report
        // "cancelled/no output" after an already successful atomic save.
        setState(JobState::Complete);
        summary_->setText(tr("Report saved to %1. The full timeline was exported without loading it into the visual preview.").arg(output));
        summary_->setAccessibleDescription(summary_->text());
        stateLabel_->setText(tr("Report saved"));
    });
    connect(worker_, &AnalysisWorker::cancelledByUser, this, [this, preserveResults, generation] {
        if (generation != jobGeneration_) return;
        releaseWorker();
        setState(preserveResults ? JobState::Complete : JobState::Cancelled);
        if (preserveResults) stateLabel_->setText(tr("Export cancelled; prior results kept"));
    });
    connect(worker_, &AnalysisWorker::failed, this, [this, generation](const QString& message) {
        if (generation != jobGeneration_) return;
        releaseWorker();
        setState(JobState::Failed);
        showError(tr("Analysis"), message);
    });
    worker_->start();
}

// The worker emits its result as its last act, so wait() returns almost immediately; this
// guarantees the QThread is never destroyed while still running.
void MainWindow::releaseWorker() {
    if (!worker_) return;
    worker_->wait();
    worker_->deleteLater();
    worker_ = nullptr;
}

void MainWindow::cancelAnalysis() {
    if (worker_ && humtrace::ui::is_active(state_)) {
        worker_->requestCancel();
        setState(JobState::Cancelling);
    }
}

void MainWindow::onFinished(std::shared_ptr<humtrace::AnalysisResult> result) {
    releaseWorker();
    if (state_ == JobState::Cancelling) { // cancel raced with completion: honor the user's intent
        setState(JobState::Cancelled);
        return;
    }
    result_ = std::make_unique<humtrace::AnalysisResult>(std::move(*result));
    try {
    if (result_->storage) paged_ = std::make_unique<PagedData>(result_->storage, 6 * 1024 * 1024);
    const auto& r = result_->report;
    // The file may have changed since it was opened: trust the analyzed channel count.
    if (channels_->count() != int(r.channels)) {
        channels_->clear();
        for (int c = 1; c <= int(r.channels); ++c) channels_->addItem(tr("Channel %1").arg(c));
    }
    spectrumInfo_->setText(spectrumMethod(r));
    spectrumInfo_->setAccessibleDescription(spectrumInfo_->text());
    if (paged_) loadFindingPage(0);
    else { findings_ = humtrace::ui::build_findings(r); model_->setRows(findings_); }
    const double hop = double(r.hop_size_samples) / r.sample_rate_hz;
    view_->reset(r.duration_seconds, hop);
    view_->setChannel(std::min(restoreChannel_, int(r.channels) - 1));
    waveform_->setViews(&result_->views, r.duration_seconds, r.sample_rate_hz);
    if (paged_) spectrogram_->setPagedData(paged_.get(), &result_->views, &r, &findings_);
    else spectrogram_->setData(&result_->views, &r, &findings_);
    applyDisplayOptions();
    std::size_t trackCount = findings_.size();
    if (paged_) { trackCount = 0; for (std::size_t c = 0; c < r.channels; ++c) trackCount += static_cast<std::size_t>(paged_->storage().track_count(c)); }
    const auto text = QString::fromStdString(humtrace::ui::summarize_report(r, trackCount));
    summary_->setText(text + tr(" SHA-256: ") + QString::fromStdString(r.input_sha256));
    summary_->setAccessibleDescription(summary_->text());
    for (QWidget* w : std::initializer_list<QWidget*>{waveform_, spectrogram_}) static_cast<TimeChart*>(w)->setAccessibleSummary(text);
    header_->setText(tr("%1 | %2 s | %3 Hz | %4 channel(s) | %5-bit %6 | SHA-256 %7")
        .arg(header_->fontMetrics().elidedText(QFileInfo(path_).fileName(), Qt::ElideMiddle, 420), QString::number(r.duration_seconds, 'f', 2),
             QString::number(r.sample_rate_hz), QString::number(r.channels), QString::number(r.bit_depth),
             r.format_code == 3 ? tr("float") : tr("PCM"), QString::fromStdString(r.input_sha256).left(16) + "..."));
    header_->setAccessibleDescription(header_->text());
    setState(JobState::Complete);
    updateFrameViews();
    // Auto-select the first finding on the restored channel (never switch the user's channel).
    for (int i = 0; i < model_->rowCount(); ++i)
        if (model_->row(i).channel == view_->channel() + 1) {
            table_->selectRow(proxy_->mapFromSource(model_->index(i, 0)).row());
            break;
        }
    } catch (const std::exception& error) { dataReadFailed(QString::fromUtf8(error.what())); }
}

std::size_t MainWindow::pagedCacheBytes() const { return paged_ ? paged_->cachedBytesUpperBound() : 0; }

void MainWindow::dataReadFailed(const QString& message) {
    if (!result_ && state_ == JobState::Failed) return;
    if (worker_) { ++jobGeneration_; worker_->requestCancel(); releaseWorker(); }
    clearResults();
    setState(JobState::Failed);
    showError(tr("Stored analysis"), message);
}

void MainWindow::loadFindingPage(std::uint64_t first) {
    if (!paged_ || !result_) return;
    try {
        const auto channel = static_cast<std::size_t>(std::max(0, view_->channel()));
        const auto count = paged_->storage().track_count(channel);
        findingPage_ = std::min(first, count ? ((count - 1) / 256) * 256 : 0);
        findings_.clear();
        const auto end = std::min(count, findingPage_ + 256);
        for (auto i = findingPage_; i < end; ++i) findings_.push_back(paged_->finding(channel, i));
        model_->setRows(findings_);
        findingPageLabel_->setText(tr("Channel %1: tracks %2–%3 of %4. Sort applies to this page.")
            .arg(channel + 1).arg(count ? findingPage_ + 1 : 0).arg(end).arg(count));
        prevPage_->setEnabled(findingPage_ > 0); nextPage_->setEnabled(end < count);
        inspector_->setPlainText(tr("Select a finding to see measurements, evidence, settings and limitations."));
        spectrogram_->setSelectedFinding(-1);
    } catch (const std::exception& error) {
        const auto message = QString::fromUtf8(error.what());
        QTimer::singleShot(0, this, [this, message] { dataReadFailed(message); });
    }
}

void MainWindow::applyDisplayOptions() { displayTimer_->start(); }

void MainWindow::applyDisplayOptionsNow() {
    displayTimer_->stop();
    // Keep floor < ceiling in the controls themselves so every view gets the same, valid range.
    const QSignalBlocker blockFloor(floorBox_), blockCeiling(ceilingBox_);
    floorBox_->setMaximum(std::min(-20.0, ceilingBox_->value() - 1.0));
    ceilingBox_->setMinimum(std::max(-100.0, floorBox_->value() + 1.0));
    spectrogram_->setDisplay(scaleBox_->currentIndex() == 0,
                             paletteBox_->currentIndex() == 0 ? humtrace::ui::Palette::Default : humtrace::ui::Palette::Grayscale,
                             floorBox_->value(), ceilingBox_->value());
    updateFrameViews();
}

int MainWindow::currentFrame() const {
    if (!result_) return -1;
    const auto& r = result_->report;
    if (view_->channel() < 0 || view_->channel() >= int(r.channel_results.size())) return -1;
    const auto& frames = r.channel_results[std::size_t(view_->channel())].frames;
    const auto count = paged_ ? paged_->storage().frame_count(static_cast<std::size_t>(view_->channel())) : frames.size();
    if (count == 0) return -1;
    const double center = view_->cursor() * r.sample_rate_hz - double(r.frame_size_samples) / 2.0;
    const long idx = std::lround(center / double(r.hop_size_samples));
    return int(std::clamp<long>(idx, 0, long(count) - 1));
}

void MainWindow::onCursorChanged() { updateFrameViews(); }

void MainWindow::updateFrameViews() {
    const int f = currentFrame();
    if (f < 0 || !result_) return;
    const auto& r = result_->report;
    if (view_->channel() < 0 || view_->channel() >= int(result_->views.size())) return;
    const auto ch = std::size_t(view_->channel());
    try {
    const auto& v = result_->views[ch];
    if (paged_) selectedRow_ = paged_->row(ch, static_cast<std::uint64_t>(f));
    const auto* row = paged_ ? &selectedRow_ : &v.spectrogram_dbfs[std::size_t(f)];
    spectrum_->setFrame(row, &v.row_frequency_hz, r.sample_rate_hz / 2.0, floorBox_->value(), ceilingBox_->value());
    const auto& frame = paged_ ? paged_->frame(ch, static_cast<std::uint64_t>(f)) : r.channel_results[ch].frames[std::size_t(f)];
    frameTable_->setRowCount(0);
    auto add = [&](const QString& kind, const QString& hz, double level) {
        const int row = frameTable_->rowCount();
        frameTable_->insertRow(row);
        const QStringList values{kind, hz, std::isfinite(level) ? QString::number(level, 'f', 2) : tr("silence")};
        for (int column = 0; column < 3; ++column) {
            auto* item = new QTableWidgetItem(values[column]);
            item->setData(Qt::AccessibleTextRole, tr("%1: %2; %3; channel %4, frame %5")
                .arg(frameTable_->horizontalHeaderItem(column)->text(), values[column], kind).arg(ch + 1).arg(f));
            frameTable_->setItem(row, column, item);
        }
    };
    const QString noteLabel = frame.strongest_peak.frequency_hz > 0.0
        ? QString::fromStdString(" [" + humtrace::format_note(humtrace::nearest_note(frame.strongest_peak.frequency_hz)) + "]")
        : QString();
    const auto& peak = frame.strongest_peak;
    const bool havePeak = peak.frequency_hz > 0.0 && std::isfinite(peak.level_dbfs);
    add(havePeak ? tr("Frame %1 strongest non-DC peak (%2)%3").arg(f)
            .arg(peak.interpolation_applied ? tr("interpolation applied") : tr("interpolation not applied"), noteLabel)
        : tr("Frame %1 no non-DC peak in search band").arg(f),
        havePeak ? QString::number(peak.frequency_hz, 'f', 3) : tr("unavailable"), peak.level_dbfs);
    QString details = tr("No non-DC peak in the search band.");
    if (havePeak) {
        details = tr("Peak: raw bin %1 Hz; interpolation %2; Hann main-lobe width %3 Hz (first-zero to first-zero); half-power width %4 Hz. "
                     "Local median floor %5 dBFS; prominence %6 dB. Frequency uncertainty %7. Flags: %8.")
            .arg(QString::number(peak.raw_bin_frequency_hz, 'f', 3), peak.interpolation_applied ? tr("applied") : tr("not applied"),
                 QString::number(peak.hann_main_lobe_width_hz, 'f', 3),
                 peak.width_hz ? QString::number(*peak.width_hz, 'f', 3) : tr("unavailable"),
                 peak.local_floor_available ? QString::number(peak.local_floor_dbfs, 'f', 2) : tr("unavailable"),
                 peak.local_floor_available ? QString::number(peak.prominence_db, 'f', 2) : tr("unavailable"),
                 peak.frequency_uncertainty_hz ? QString::number(*peak.frequency_uncertainty_hz, 'f', 3) + " Hz" : tr("is not estimated"),
                 peak.boundary_peak || peak.competing_peak ?
                    (peak.boundary_peak ? tr("boundary peak; ") : QString()) + (peak.competing_peak ? tr("nearby competing peak") : QString()) : tr("none flagged"));
    }
    spectrumInfo_->setText(spectrumMethod(r) + "\n" + details);
    spectrumInfo_->setAccessibleDescription(spectrumInfo_->text());
    for (int column = 0; column < 3; ++column) {
        frameTable_->item(0, column)->setData(Qt::AccessibleDescriptionRole, details);
        frameTable_->item(0, column)->setToolTip(details);
    }
    for (const auto* cand : {&frame.mains_50, &frame.mains_60})
        for (const auto& h : cand->harmonics)
            add(tr("%1 Hz harmonic %2 bin%3").arg(cand->nominal_frequency_hz, 0, 'f', 0).arg(h.harmonic_number)
                    .arg(h.supports_threshold ? tr(" (above threshold)") : QString()), QString::number(h.measured_bin_hz, 'f', 3), h.level_dbfs);
    for (std::size_t i = 0; i < row->size(); ++i) {
        const double endHz = i + 1 < v.row_frequency_hz.size() ? v.row_frequency_hz[i + 1] : r.sample_rate_hz / 2.0;
        add(tr("Display row %1 maximum FFT bin (floor-clamped)").arg(i + 1),
            tr("%1 to %2").arg(v.row_frequency_hz[i], 0, 'f', 3).arg(endHz, 0, 'f', 3), (*row)[i]);
    }
    frameTable_->resizeColumnsToContents();
    } catch (const std::exception& error) { dataReadFailed(QString::fromUtf8(error.what())); }
}

void MainWindow::onFindingSelected() {
    const auto index = proxy_->mapToSource(table_->currentIndex());
    if (!index.isValid() || !result_) return;
    try {
    const auto row = model_->row(index.row());
    if (paged_) {
        const auto track = paged_->track(static_cast<std::size_t>(row.channel - 1), findingPage_ + static_cast<std::uint64_t>(index.row()));
        inspector_->setPlainText(QString::fromStdString(humtrace::ui::describe_finding(result_->report, row, &track.evidence)));
    } else inspector_->setPlainText(QString::fromStdString(humtrace::ui::describe_finding(result_->report, row)));
    view_->setChannel(row.channel - 1);
    view_->setSelection(row.start_seconds, row.end_seconds);
    view_->setCursor(row.start_seconds + double(result_->report.frame_size_samples) / 2.0 / result_->report.sample_rate_hz);
    if (row.start_seconds < view_->view().start || row.end_seconds > view_->view().end)
        view_->setView({row.start_seconds - row.duration_seconds() * 0.25, row.end_seconds + row.duration_seconds() * 0.25});
    spectrogram_->setSelectedFinding(index.row());
    } catch (const std::exception& error) { dataReadFailed(QString::fromUtf8(error.what())); }
}

void MainWindow::stepFinding(int delta) {
    const int rows = proxy_->rowCount();
    if (rows == 0) return;
    const int current = table_->currentIndex().isValid() ? table_->currentIndex().row() : (delta > 0 ? -1 : rows);
    if (paged_ && current + delta >= rows && nextPage_->isEnabled()) {
        loadFindingPage(findingPage_ + 256); if (model_->rowCount()) table_->selectRow(0); return;
    }
    if (paged_ && current + delta < 0 && findingPage_ >= 256) {
        loadFindingPage(findingPage_ - 256); if (model_->rowCount()) table_->selectRow(model_->rowCount() - 1); return;
    }
    table_->selectRow(std::clamp(current + delta, 0, rows - 1));
}

void MainWindow::chooseAndOpen() {
    const auto path = QFileDialog::getOpenFileName(this, tr("Open recording"), {}, tr("WAV audio (*.wav *.wave);;All files (*)"));
    if (!path.isEmpty()) openRecording(path);
}

void MainWindow::chooseAndStreamExport() {
    const auto output = QFileDialog::getSaveFileName(this, tr("Export long-file report"),
        QFileInfo(path_).completeBaseName() + "-analysis.json", tr("JSON report (*.json)"));
    if (!output.isEmpty()) startStreamingExport(output, true); // save dialog confirms replacement
}

QString MainWindow::exportReport(const QString& path, bool redactPath) {
    if (!result_) return tr("Nothing to export");
    if (result_->storage) {
        try { result_->storage->export_json(path.toStdString(), true, !redactPath); }
        catch (const std::exception& error) { return QString::fromUtf8(error.what()); }
        return {};
    }
    auto report = result_->report;
    if (redactPath) report.input_path = QFileInfo(path_).fileName().toStdString();
    try {
        humtrace::require_distinct_paths(path.toStdString(), path_.toStdString());
        humtrace::write_file_atomically(path.toStdString(), humtrace::serialize_report_json(report));
    } catch (const std::exception& e) {
        return QString::fromUtf8(e.what());
    }
    return {};
}

void MainWindow::chooseAndExport() {
    if (!result_) return;
    const auto& r = result_->report;
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Export preview"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* info = new QLabel(tr("Schema version %1 (JSON)\nInput SHA-256: %2\nStatus: complete, %3 frames per channel, %4 track(s)\n"
                               "Warnings: %5\nThis file records measurements only; it is not a chain-of-custody record.")
                                .arg(QString::fromLatin1(humtrace::kReportSchemaVersion), QString::fromStdString(r.input_sha256)).arg(paged_ ? paged_->storage().frame_count(0) : (r.channel_results.empty() ? 0 : r.channel_results[0].frames.size())).arg([&] { std::uint64_t count = findings_.size(); if (paged_) { count = 0; for (std::size_t c = 0; c < r.channels; ++c) count += paged_->storage().track_count(c); } return count; }())
                                .arg(r.warnings.empty() ? tr("none recorded (clipping and hidden short tracks are checked)") : [&] { QStringList l; for (const auto& w : r.warnings) l << QString::fromStdString(w); return l.join("; "); }()));
    info->setWordWrap(true);
    info->setTextInteractionFlags(Qt::TextSelectableByKeyboard);
    auto* redact = new QCheckBox(tr("Store the file name only (omit the directory path)"));
    redact->setChecked(true);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(info);
    layout->addWidget(redact);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto path = QFileDialog::getSaveFileName(this, tr("Export report"), QFileInfo(path_).completeBaseName() + ".humtrace.json", tr("JSON (*.json)"));
    if (path.isEmpty()) return;
    if (result_->storage) {
        startWorker(new AnalysisWorker(result_->storage, path, true, !redact->isChecked(), this), true);
        return;
    }
    const auto error = exportReport(path, redact->isChecked());
    if (!error.isEmpty()) QMessageBox::warning(this, tr("HumTrace"), tr("Export failed: %1\nNo partial file was left in place.").arg(error));
    else statusBar()->showMessage(tr("Report saved: %1").arg(path), 5000);
}

void MainWindow::chooseAndCompare() {
    if (!result_ || compareBusy_) return;
    const auto other = QFileDialog::getOpenFileName(this, tr("Compare with recording B"), {}, tr("WAV audio (*.wav *.wave)"));
    if (other.isEmpty()) return;
    QProgressDialog progress(tr("Analyzing recording B..."), tr("Cancel"), 0, 1000, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false); progress.setAutoReset(false);
    AnalysisWorker worker(other, settings_, nullptr, forcePaged_, streamLimits_);
    comparisonAnalysis_ = &worker;
    std::shared_ptr<humtrace::AnalysisResult> done;
    QString failure;
    bool cancelled = false;
    QEventLoop loop;
    compareBusy_ = true;
    connect(&worker, &AnalysisWorker::stageChanged, &progress, [&](int stage, double f) { progress.setLabelText(tr("Recording B — %1").arg(stageText(stage))); progress.setValue(int(f * 1000)); });
    connect(&worker, &AnalysisWorker::resultReady, &loop, [&](std::shared_ptr<humtrace::AnalysisResult> r) { done = r; loop.quit(); });
    connect(&worker, &AnalysisWorker::cancelledByUser, &loop, [&] { cancelled = true; loop.quit(); });
    connect(&worker, &AnalysisWorker::failed, &loop, [&](QString m) { failure = m; loop.quit(); });
    connect(&progress, &QProgressDialog::canceled, &worker, &AnalysisWorker::requestCancel);
    worker.start();
    loop.exec();
    worker.wait();
    comparisonAnalysis_ = nullptr;
    if (!failure.isEmpty() || cancelled || !done || progress.wasCanceled()) {
        progress.close(); compareBusy_ = false;
        if (!failure.isEmpty()) showError(tr("Comparison"), failure);
        return;
    }
    progress.setLabelText(tr("Comparing measured track summaries...")); progress.setRange(0, 0);
    auto left = std::make_shared<humtrace::AnalysisReport>(result_->report);
    auto right = std::make_shared<humtrace::AnalysisReport>(std::move(done->report));
    const auto leftStorage = result_->storage, rightStorage = done->storage;
    std::optional<humtrace::ComparisonReport> comparison;
    std::unique_ptr<QThread> task(QThread::create([&, left, right, leftStorage, rightStorage] {
        try {
            const auto stop = [] { return QThread::currentThread()->isInterruptionRequested(); };
            if (leftStorage) *left = humtrace::comparison_summary(*leftStorage, stop);
            if (rightStorage) *right = humtrace::comparison_summary(*rightStorage, stop);
            comparison = humtrace::compare_reports(*left, *right, {}, stop);
        } catch (const humtrace::AnalysisCancelled&) { cancelled = true; }
        catch (const std::exception& error) { failure = QString::fromUtf8(error.what()); }
    }));
    comparisonThread_ = task.get();
    connect(task.get(), &QThread::finished, &loop, &QEventLoop::quit);
    connect(&progress, &QProgressDialog::canceled, task.get(), &QThread::requestInterruption);
    task->start(); loop.exec(); task->wait(); comparisonThread_ = nullptr;
    const bool userCancelled = cancelled || progress.wasCanceled();
    progress.close(); compareBusy_ = false;
    if (!failure.isEmpty()) { showError(tr("Comparison"), failure); return; }
    if (!userCancelled && comparison) ComparisonDialog(*comparison, this).exec();
}

void MainWindow::showSettings() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Analysis settings"));
    dialog.resize(560, 600);
    auto* layout = new QVBoxLayout(&dialog);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* content = new QWidget;
    auto* form = new QFormLayout(content);
    scroll->setWidget(content);
    layout->addWidget(scroll);
    auto spin = [&](double lo, double hi, double v, const QString& suffix, const QString& name) {
        auto* s = new QDoubleSpinBox;
        s->setRange(lo, hi);
        s->setDecimals(3);
        s->setValue(v);
        s->setSuffix(suffix);
        s->setAccessibleName(name);
        return s;
    };
    auto integer = [&](int lo, int hi, std::size_t value, const QString& name) {
        auto* s = new QSpinBox;
        s->setRange(lo, hi);
        s->setValue(static_cast<int>(std::min(value, static_cast<std::size_t>(hi))));
        s->setAccessibleName(name);
        return s;
    };
    auto powers = [&](int lo, int hi, std::size_t value, const QString& name, bool automatic = false) {
        auto* s = new QComboBox;
        if (automatic) s->addItem(tr("Automatic"), 0);
        for (int v = lo; v <= hi; v *= 2) s->addItem(QString::number(v), v);
        s->setCurrentIndex(std::max(0, s->findData(static_cast<int>(value))));
        s->setAccessibleName(name);
        return s;
    };
    auto check = [&](bool value, const QString& name) {
        auto* s = new QCheckBox(name);
        s->setAccessibleName(name);
        s->setChecked(value);
        return s;
    };
    auto* frameSize = powers(16, 65536, settings_.frame_size_samples, tr("Analysis frame size"), true);
    auto* hopSize = integer(0, 65536, settings_.hop_size_samples, tr("Analysis hop size"));
    hopSize->setSpecialValueText(tr("Automatic (half window)"));
    auto* removeMean = check(settings_.spectrum_remove_mean, tr("Remove spectrum mean"));
    auto* searchMin = spin(0, 1e9, settings_.peak_search.minimum_frequency_hz, " Hz", tr("Minimum search frequency"));
    auto* searchMax = spin(0, 1e9, settings_.peak_search.maximum_frequency_hz, " Hz", tr("Maximum search frequency"));
    searchMax->setSpecialValueText(tr("Nyquist"));
    auto* separation = spin(0, 1e9, settings_.peak_search.minimum_separation_hz, " Hz", tr("Minimum peak separation"));
    auto* guard = integer(0, 1048575, settings_.peak_search.floor_guard_bins, tr("Local floor guard bins"));
    auto* radius = integer(1, 1048576, settings_.peak_search.floor_radius_bins, tr("Local floor radius bins"));
    connect(guard, &QSpinBox::valueChanged, radius, [radius](int value) { radius->setMinimum(value + 1); });
    radius->setMinimum(guard->value() + 1);
    auto* harmonicTolerance = spin(0, 1e9, settings_.harmonic_tolerance_hz, " Hz", tr("Harmonic search tolerance"));
    harmonicTolerance->setSpecialValueText(tr("Nominal bins (legacy)"));
    auto* welchSize = powers(4, 1048576, settings_.welch.segment_size, tr("Welch segment size"));
    auto* welchHop = integer(1, static_cast<int>(settings_.welch.segment_size), settings_.welch.hop_size, tr("Welch hop size"));
    connect(welchSize, &QComboBox::currentIndexChanged, welchHop, [welchSize, welchHop] { welchHop->setMaximum(welchSize->currentData().toInt()); });
    auto* welchMean = check(settings_.welch.remove_mean, tr("Remove Welch mean"));
    auto* threshold = spin(-160, 0, settings_.support_threshold_dbfs, " dBFS", tr("Harmonic support threshold"));
    auto* prominence = spin(0, 80, settings_.minimum_prominence_db, " dB", tr("Minimum local prominence"));
    auto* tolerance = spin(0, 50, settings_.tracker.fixed_hz_tolerance, " Hz", tr("Track match tolerance"));
    auto* fraction = spin(0, 8, settings_.tracker.bin_fraction, " bins", tr("Track match radius in FFT bins"));
    auto* relative = spin(0, 1e6, settings_.tracker.relative_tolerance, "", tr("Relative track match tolerance"));
    auto* peakLevel = spin(-160, 0, settings_.peak_min_level_dbfs, " dBFS", tr("Minimum peak level"));
    auto* peakProm = spin(0, 80, settings_.peak_min_prominence_db, " dB", tr("Minimum peak prominence"));
    auto* gap = new QSpinBox;
    gap->setRange(0, 20);
    gap->setValue(int(settings_.tracker.max_gap_frames));
    gap->setAccessibleName(tr("Maximum gap frames"));
    auto* minFrames = new QSpinBox;
    minFrames->setRange(1, 100);
    minFrames->setValue(int(settings_.tracker.min_observed_frames));
    minFrames->setAccessibleName(tr("Minimum observed frames per track"));
    auto* peaks = new QSpinBox;
    peaks->setRange(1, 32);
    peaks->setValue(int(settings_.max_peaks_per_frame));
    peaks->setAccessibleName(tr("Peaks per frame"));
    form->addRow(tr("Analysis frame size (samples)"), frameSize);
    form->addRow(tr("Analysis hop size (samples)"), hopSize);
    form->addRow(removeMean);
    form->addRow(tr("Minimum search frequency"), searchMin);
    form->addRow(tr("Maximum search frequency"), searchMax);
    form->addRow(tr("Minimum peak separation"), separation);
    form->addRow(tr("Local floor guard (bins)"), guard);
    form->addRow(tr("Local floor radius (bins)"), radius);
    form->addRow(tr("Harmonic search tolerance"), harmonicTolerance);
    form->addRow(tr("Harmonic support threshold"), threshold);
    form->addRow(tr("Minimum local prominence"), prominence);
    form->addRow(tr("Minimum peak level"), peakLevel);
    form->addRow(tr("Minimum peak prominence"), peakProm);
    form->addRow(tr("Peaks per frame"), peaks);
    form->addRow(tr("Track match tolerance"), tolerance);
    form->addRow(tr("Track match radius"), fraction);
    form->addRow(tr("Relative track tolerance (fraction)"), relative);
    form->addRow(tr("Gap tolerance (frames)"), gap);
    form->addRow(tr("Minimum frames per track"), minFrames);
    form->addRow(tr("Welch segment size (samples)"), welchSize);
    form->addRow(tr("Welch hop size (samples)"), welchHop);
    form->addRow(welchMean);
    form->addRow(new QLabel(tr("Analysis settings mark results stale; re-run Analyze to apply.")));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    auto next = settings_;
    next.frame_size_samples = static_cast<std::size_t>(frameSize->currentData().toInt());
    next.hop_size_samples = static_cast<std::size_t>(hopSize->value());
    next.spectrum_remove_mean = removeMean->isChecked();
    next.peak_search.minimum_frequency_hz = searchMin->value();
    next.peak_search.maximum_frequency_hz = searchMax->value();
    next.peak_search.minimum_separation_hz = separation->value();
    next.peak_search.floor_guard_bins = static_cast<std::size_t>(guard->value());
    next.peak_search.floor_radius_bins = static_cast<std::size_t>(radius->value());
    next.harmonic_tolerance_hz = harmonicTolerance->value();
    next.welch.segment_size = static_cast<std::size_t>(welchSize->currentData().toInt());
    next.welch.hop_size = static_cast<std::size_t>(welchHop->value());
    next.welch.remove_mean = welchMean->isChecked();
    next.support_threshold_dbfs = threshold->value();
    next.minimum_prominence_db = prominence->value();
    next.peak_min_level_dbfs = peakLevel->value();
    next.peak_min_prominence_db = peakProm->value();
    next.max_peaks_per_frame = std::size_t(peaks->value());
    next.tracker.fixed_hz_tolerance = tolerance->value();
    next.tracker.bin_fraction = fraction->value();
    next.tracker.relative_tolerance = relative->value();
    next.tracker.max_gap_frames = std::size_t(gap->value());
    next.tracker.min_observed_frames = std::size_t(minFrames->value());
    try { humtrace::validate_settings(next); }
    catch (const std::exception& error) { showError(tr("Settings"), QString::fromUtf8(error.what())); return; }
    setPipelineSettings(next);
}

void MainWindow::setPipelineSettings(const humtrace::PipelineSettings& next) {
    if (sameSettings(settings_, next)) return;
    settings_ = next;
    if (state_ == JobState::Complete) {
        setState(JobState::Invalidated);
        summary_->setText(tr("Settings changed: these results are stale. Re-run Analyze (Ctrl+R)."));
    }
}

void MainWindow::setDisplayOptions(int scaleIndex, int paletteIndex, double floorDb, double ceilingDb) {
    scaleBox_->setCurrentIndex(scaleIndex);
    paletteBox_->setCurrentIndex(paletteIndex);
    // Set the wider-range value first so the pair is always accepted regardless of order.
    floorBox_->setMaximum(0.0);
    ceilingBox_->setMinimum(-160.0);
    floorBox_->setValue(floorDb);
    ceilingBox_->setValue(ceilingDb);
    applyDisplayOptionsNow();
}

void MainWindow::showHelp() {
    QMessageBox::information(this, tr("HumTrace keyboard help and limitations"),
        tr("Ctrl+O open  |  Ctrl+R analyze  |  Ctrl+. cancel  |  Ctrl+E export  |  Ctrl+Shift+C compare  |  Ctrl+, settings\n"
           "Ctrl+Down / Ctrl+Up: next / previous finding\n\n"
           "In a chart: Left/Right move the cursor (Ctrl = 10x, Shift = extend selection); +/- zoom; 0 reset zoom; "
           "Home/End; Esc clears the selection. Mouse wheel zooms, horizontal scroll pans, drag selects.\n\n"
           "Limitations: levels are dBFS from decoded samples, not SPL. A frequency pattern does not identify a device, "
           "ground loop, location, recording date, or authenticity. Track labels are descriptive and not validated "
           "against a labeled corpus. Channels are analyzed independently; nothing is resampled, mixed, or aligned."));
}

void MainWindow::addRecent(const QString& path) {
    QSettings s;
    QStringList list = s.value("recent").toStringList();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > 8) list.removeLast();
    s.setValue("recent", list);
    loadRecent();
}

void MainWindow::loadRecent() {
    recent_->clear();
    for (const auto& p : QSettings().value("recent").toStringList()) {
        auto* item = new QListWidgetItem(QFileInfo(p).fileName(), recent_);
        item->setToolTip(p);
        item->setData(Qt::UserRole, p);
    }
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (comparisonAnalysis_) { comparisonAnalysis_->requestCancel(); comparisonAnalysis_->wait(); }
    if (comparisonThread_) { comparisonThread_->requestInterruption(); comparisonThread_->wait(); }
    if (worker_) {
        worker_->requestCancel();
        worker_->wait();
    }
    e->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}
void MainWindow::dropEvent(QDropEvent* e) {
    const auto urls = e->mimeData()->urls();
    if (!urls.isEmpty() && urls.first().isLocalFile()) openRecording(urls.first().toLocalFile());
}
