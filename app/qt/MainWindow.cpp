#include "MainWindow.hpp"

#include "AnalysisWorker.hpp"
#include "ComparisonDialog.hpp"
#include "FindingsModel.hpp"
#include "SpectrogramChart.hpp"
#include "SpectrumChart.hpp"
#include "Theme.hpp"
#include "ViewState.hpp"
#include "WaveformChart.hpp"
#include "humtrace/compare.hpp"
#include "humtrace/wav.hpp"

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
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTextBrowser>
#include <QToolBar>
#include <QVBoxLayout>
#include <fstream>

using humtrace::ui::JobState;

namespace {
QString stageText(int stage) {
    switch (static_cast<humtrace::Stage>(stage)) {
    case humtrace::Stage::Hashing: return QObject::tr("Hashing input");
    case humtrace::Stage::Analyzing: return QObject::tr("Analyzing");
    case humtrace::Stage::Done: return QObject::tr("Finalizing");
    }
    return {};
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
    connect(floorBox_, &QDoubleSpinBox::valueChanged, this, &MainWindow::applyDisplayOptions);
    connect(ceilingBox_, &QDoubleSpinBox::valueChanged, this, &MainWindow::applyDisplayOptions);

    waveform_ = new WaveformChart(view_);
    spectrogram_ = new SpectrogramChart(view_);
    spectrum_ = new SpectrumChart;
    frameTable_ = new QTableWidget(0, 3);
    frameTable_->setHorizontalHeaderLabels({tr("Measurement"), tr("Frequency (Hz)"), tr("Level (dBFS)")});
    frameTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    frameTable_->horizontalHeader()->setStretchLastSection(true);
    frameTable_->setAccessibleName(tr("Spectrum measurement table for the frame at the cursor"));
    auto* tabs = new QTabWidget;
    tabs->addTab(spectrum_, tr("Spectrum"));
    tabs->addTab(frameTable_, tr("Spectrum table"));
    tabs->setAccessibleName(tr("Selected-frame spectrum views"));
    layout->addWidget(waveform_, 2);
    layout->addWidget(spectrogram_, 4);
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
    rl2->addWidget(table_, 2);
    rl2->addWidget(inspector_, 3);
    right->setWidget(rw);
    addDockWidget(Qt::RightDockWidgetArea, right);
    right->setMinimumWidth(420);
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
    file->addActions({openAct_, exportAct_});
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
    settingsAct_->setEnabled(!active);
    nextAct_->setEnabled(model_ && model_->rowCount() > 0);
    prevAct_->setEnabled(model_ && model_->rowCount() > 0);
    progress_->setVisible(active);
}

void MainWindow::showError(const QString& stage, const QString& message) {
    if (errorHandler_) { errorHandler_(stage, message); return; }
    QMessageBox::warning(this, tr("HumTrace"),
        tr("%1 failed for \"%2\".\n\n%3\n\nThe original file was not modified. Check that it is a supported WAV file and try again.")
            .arg(stage, QFileInfo(path_).fileName(), message));
}

void MainWindow::clearResults() {
    result_.reset();
    findings_.clear();
    model_->setRows({});
    waveform_->setViews(nullptr, 0);
    spectrogram_->setData(nullptr, nullptr, nullptr);
    spectrum_->setFrame(nullptr, nullptr, 0, -100, 0);
    frameTable_->setRowCount(0);
    summary_->clear();
    inspector_->setPlainText(tr("Select a finding to see measurements, evidence, settings and limitations."));
    view_->reset(0.0, 1.0);
}

void MainWindow::openRecording(const QString& path) {
    if (humtrace::ui::is_active(state_)) return;
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
    header_->setText(tr("%1 | %2 s | %3 Hz | %4 channel(s) | %5-bit %6 | SHA-256 computed during analysis")
        .arg(QFileInfo(path).fileName(), QString::number(double(m.frame_count) / m.sample_rate_hz, 'f', 2),
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
    if (path_.isEmpty() || humtrace::ui::is_active(state_)) return;
    clearResults();
    setState(JobState::Hashing);
    progress_->setRange(0, 1000);
    progress_->setValue(0);
    worker_ = new AnalysisWorker(path_, settings_, this);
    connect(worker_, &AnalysisWorker::stageChanged, this, [this](int stage, double fraction) {
        const auto next = static_cast<humtrace::Stage>(stage) == humtrace::Stage::Hashing ? JobState::Hashing
                          : static_cast<humtrace::Stage>(stage) == humtrace::Stage::Analyzing ? JobState::Analyzing
                                                                                               : JobState::Finalizing;
        if (state_ != JobState::Cancelling && state_ != next) setState(next);
        progress_->setValue(int(fraction * 1000));
        stateLabel_->setText(tr("%1: %2%").arg(stageText(stage)).arg(int(fraction * 100)));
    });
    connect(worker_, &AnalysisWorker::finished, this, &MainWindow::onFinished);
    connect(worker_, &AnalysisWorker::cancelledByUser, this, [this] {
        releaseWorker();
        setState(JobState::Cancelled);
    });
    connect(worker_, &AnalysisWorker::failed, this, [this](const QString& message) {
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
    const auto& r = result_->report;
    // The file may have changed since it was opened: trust the analyzed channel count.
    if (channels_->count() != int(r.channels)) {
        channels_->clear();
        for (int c = 1; c <= int(r.channels); ++c) channels_->addItem(tr("Channel %1").arg(c));
    }
    findings_ = humtrace::ui::build_findings(r);
    model_->setRows(findings_);
    const double hop = double(r.hop_size_samples) / r.sample_rate_hz;
    view_->reset(r.duration_seconds, hop);
    view_->setChannel(std::min(channels_->currentRow() < 0 ? 0 : channels_->currentRow(), int(r.channels) - 1));
    waveform_->setViews(&result_->views, r.duration_seconds, r.sample_rate_hz);
    spectrogram_->setData(&result_->views, &r, &findings_);
    applyDisplayOptions();
    const auto text = QString::fromStdString(humtrace::ui::summarize_report(r, findings_));
    summary_->setText(text + tr(" SHA-256: ") + QString::fromStdString(r.input_sha256));
    for (QWidget* w : std::initializer_list<QWidget*>{waveform_, spectrogram_}) static_cast<TimeChart*>(w)->setAccessibleSummary(text);
    header_->setText(header_->text().replace(tr("SHA-256 computed during analysis"), tr("SHA-256 %1").arg(QString::fromStdString(r.input_sha256).left(16) + "...")));
    setState(JobState::Complete);
    updateFrameViews();
    if (model_->rowCount() > 0) table_->selectRow(0);
}

void MainWindow::applyDisplayOptions() {
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
    if (frames.empty()) return -1;
    const double center = view_->cursor() * r.sample_rate_hz - double(r.frame_size_samples) / 2.0;
    const long idx = std::lround(center / double(r.hop_size_samples));
    return int(std::clamp<long>(idx, 0, long(frames.size()) - 1));
}

void MainWindow::onCursorChanged() { updateFrameViews(); }

void MainWindow::updateFrameViews() {
    const int f = currentFrame();
    if (f < 0 || !result_) return;
    const auto& r = result_->report;
    if (view_->channel() < 0 || view_->channel() >= int(result_->views.size())) return;
    const auto ch = std::size_t(view_->channel());
    const auto& v = result_->views[ch];
    spectrum_->setFrame(&v.spectrogram_dbfs[std::size_t(f)], &v.row_frequency_hz, r.sample_rate_hz / 2.0, floorBox_->value(), ceilingBox_->value());
    const auto& frame = r.channel_results[ch].frames[std::size_t(f)];
    frameTable_->setRowCount(0);
    auto add = [&](const QString& kind, double hz, double level) {
        const int row = frameTable_->rowCount();
        frameTable_->insertRow(row);
        frameTable_->setItem(row, 0, new QTableWidgetItem(kind));
        frameTable_->setItem(row, 1, new QTableWidgetItem(QString::number(hz, 'f', 3)));
        frameTable_->setItem(row, 2, new QTableWidgetItem(std::isfinite(level) ? QString::number(level, 'f', 2) : tr("silence")));
    };
    add(tr("Frame %1 strongest non-DC peak (interpolated)").arg(f), frame.strongest_peak.frequency_hz, frame.strongest_peak.level_dbfs);
    for (const auto* cand : {&frame.mains_50, &frame.mains_60})
        for (const auto& h : cand->harmonics)
            add(tr("%1 Hz harmonic %2 bin%3").arg(cand->nominal_frequency_hz, 0, 'f', 0).arg(h.harmonic_number)
                    .arg(h.supports_threshold ? tr(" (above threshold)") : QString()), h.measured_bin_hz, h.level_dbfs);
    frameTable_->resizeColumnsToContents();
}

void MainWindow::onFindingSelected() {
    const auto index = proxy_->mapToSource(table_->currentIndex());
    if (!index.isValid() || !result_) return;
    const auto& row = model_->row(index.row());
    inspector_->setPlainText(QString::fromStdString(humtrace::ui::describe_finding(result_->report, row)));
    view_->setChannel(row.channel - 1);
    view_->setSelection(row.start_seconds, row.end_seconds);
    view_->setCursor(row.start_seconds);
    if (row.start_seconds < view_->view().start || row.end_seconds > view_->view().end)
        view_->setView({row.start_seconds - row.duration_seconds() * 0.25, row.end_seconds + row.duration_seconds() * 0.25});
    spectrogram_->setSelectedFinding(index.row());
}

void MainWindow::stepFinding(int delta) {
    const int rows = proxy_->rowCount();
    if (rows == 0) return;
    const int current = table_->currentIndex().isValid() ? table_->currentIndex().row() : (delta > 0 ? -1 : rows);
    table_->selectRow(std::clamp(current + delta, 0, rows - 1));
}

void MainWindow::chooseAndOpen() {
    const auto path = QFileDialog::getOpenFileName(this, tr("Open recording"), {}, tr("WAV audio (*.wav *.wave);;All files (*)"));
    if (!path.isEmpty()) openRecording(path);
}

QString MainWindow::exportReport(const QString& path, bool redactPath) {
    if (!result_) return tr("Nothing to export");
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
    auto* info = new QLabel(tr("Schema version 1.1 (JSON)\nInput SHA-256: %1\nStatus: complete, %2 frames per channel, %3 track(s)\n"
                               "Warnings: none\nThis file records measurements only; it is not a chain-of-custody record.")
                                .arg(QString::fromStdString(r.input_sha256)).arg(r.channel_results.empty() ? 0 : r.channel_results[0].frames.size()).arg(findings_.size()));
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
    const auto error = exportReport(path, redact->isChecked());
    if (!error.isEmpty()) QMessageBox::warning(this, tr("HumTrace"), tr("Export failed: %1\nNo partial file was left in place.").arg(error));
    else statusBar()->showMessage(tr("Report saved: %1").arg(path), 5000);
}

void MainWindow::chooseAndCompare() {
    if (!result_) return;
    const auto other = QFileDialog::getOpenFileName(this, tr("Compare with recording B"), {}, tr("WAV audio (*.wav *.wave)"));
    if (other.isEmpty()) return;
    QProgressDialog progress(tr("Analyzing recording B..."), tr("Cancel"), 0, 1000, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    AnalysisWorker worker(other, settings_);
    std::shared_ptr<humtrace::AnalysisResult> done;
    QString failure;
    bool cancelled = false;
    QEventLoop loop;
    connect(&worker, &AnalysisWorker::stageChanged, &progress, [&](int, double f) { progress.setValue(int(f * 1000)); });
    connect(&worker, &AnalysisWorker::finished, &loop, [&](std::shared_ptr<humtrace::AnalysisResult> r) { done = r; loop.quit(); });
    connect(&worker, &AnalysisWorker::cancelledByUser, &loop, [&] { cancelled = true; loop.quit(); });
    connect(&worker, &AnalysisWorker::failed, &loop, [&](QString m) { failure = m; loop.quit(); });
    connect(&progress, &QProgressDialog::canceled, &worker, &AnalysisWorker::requestCancel);
    worker.start();
    loop.exec();
    worker.wait();
    progress.close();
    if (!failure.isEmpty()) { showError(tr("Comparison"), failure); return; }
    if (cancelled || !done) return;
    const auto comparison = humtrace::compare_reports(result_->report, done->report);
    ComparisonDialog(comparison, this).exec();
}

void MainWindow::showSettings() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Analysis settings"));
    auto* form = new QFormLayout(&dialog);
    auto spin = [&](double lo, double hi, double v, const QString& suffix, const QString& name) {
        auto* s = new QDoubleSpinBox;
        s->setRange(lo, hi);
        s->setValue(v);
        s->setSuffix(suffix);
        s->setAccessibleName(name);
        return s;
    };
    auto* threshold = spin(-160, 0, settings_.support_threshold_dbfs, " dBFS", tr("Harmonic support threshold"));
    auto* prominence = spin(0, 80, settings_.minimum_prominence_db, " dB", tr("Minimum local prominence"));
    auto* tolerance = spin(0, 50, settings_.tracker.fixed_hz_tolerance, " Hz", tr("Track match tolerance"));
    auto* fraction = spin(0, 8, settings_.tracker.bin_fraction, " bins", tr("Track match radius in FFT bins"));
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
    form->addRow(tr("Harmonic support threshold"), threshold);
    form->addRow(tr("Minimum local prominence"), prominence);
    form->addRow(tr("Minimum peak level"), peakLevel);
    form->addRow(tr("Minimum peak prominence"), peakProm);
    form->addRow(tr("Peaks per frame"), peaks);
    form->addRow(tr("Track match tolerance"), tolerance);
    form->addRow(tr("Track match radius"), fraction);
    form->addRow(tr("Gap tolerance (frames)"), gap);
    form->addRow(tr("Minimum frames per track"), minFrames);
    form->addRow(new QLabel(tr("Analysis settings mark results stale; re-run Analyze to apply.")));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    auto next = settings_;
    next.support_threshold_dbfs = threshold->value();
    next.minimum_prominence_db = prominence->value();
    next.peak_min_level_dbfs = peakLevel->value();
    next.peak_min_prominence_db = peakProm->value();
    next.max_peaks_per_frame = std::size_t(peaks->value());
    next.tracker.fixed_hz_tolerance = tolerance->value();
    next.tracker.bin_fraction = fraction->value();
    next.tracker.max_gap_frames = std::size_t(gap->value());
    next.tracker.min_observed_frames = std::size_t(minFrames->value());
    const bool changed = next.support_threshold_dbfs != settings_.support_threshold_dbfs ||
        next.minimum_prominence_db != settings_.minimum_prominence_db || next.peak_min_level_dbfs != settings_.peak_min_level_dbfs ||
        next.peak_min_prominence_db != settings_.peak_min_prominence_db || next.max_peaks_per_frame != settings_.max_peaks_per_frame ||
        next.tracker.fixed_hz_tolerance != settings_.tracker.fixed_hz_tolerance || next.tracker.bin_fraction != settings_.tracker.bin_fraction ||
        next.tracker.max_gap_frames != settings_.tracker.max_gap_frames ||
        next.tracker.min_observed_frames != settings_.tracker.min_observed_frames;
    settings_ = next;
    if (changed && state_ == JobState::Complete) {
        setState(JobState::Invalidated); // results stay visible but are labeled stale; export disabled
        summary_->setText(tr("Settings changed: these results are stale. Re-run Analyze (Ctrl+R)."));
    }
}

void MainWindow::setPipelineSettings(const humtrace::PipelineSettings& next) {
    settings_ = next;
    if (state_ == JobState::Complete) {
        setState(JobState::Invalidated);
        summary_->setText(tr("Settings changed: these results are stale. Re-run Analyze (Ctrl+R)."));
    }
}

void MainWindow::setDisplayOptions(int scaleIndex, int paletteIndex, double floorDb, double ceilingDb) {
    scaleBox_->setCurrentIndex(scaleIndex);
    paletteBox_->setCurrentIndex(paletteIndex);
    floorBox_->setValue(floorDb);
    ceilingBox_->setValue(ceilingDb);
    applyDisplayOptions();
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
