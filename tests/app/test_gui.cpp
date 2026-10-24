// End-to-end GUI behavior tests (offscreen). Exercise the same code paths as the actions.
#include "FindingsModel.hpp"
#include "ComparisonDialog.hpp"
#include <QPushButton>
#include "PagedData.hpp"
#include "humtrace/report.hpp"
#include "MainWindow.hpp"
#include "SpectrogramChart.hpp"
#include "SpectrumChart.hpp"
#include "ViewState.hpp"
#include "WaveformChart.hpp"
#include "WaveformModel.hpp"
#include "humtrace/sha256.hpp"
#include "humtrace/version.hpp"

#include <QDir>
#include <QAccessible>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QProgressDialog>
#include <QSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QTableWidget>
#include <QFile>
#include <QTableView>
#include <QTextBrowser>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QHeaderView>
#include <QTabWidget>
#include <QScrollBar>
#include <cmath>
#include <fstream>
#include <numbers>

namespace {
// Intercept the queued comparison-thread completion before the nested loop handles it.
// This makes the late-cancel race deterministic without slowing or mocking the worker.
class CancelAtComparisonCompletion : public QObject {
public:
    bool cancelOnCompletion{true};
    bool cancelledAtCompletion{false};
    bool comparisonShown{false};
    bool eventFilter(QObject* receiver, QEvent* event) override {
        if (cancelOnCompletion && event->type() == QEvent::MetaCall && qobject_cast<QEventLoop*>(receiver) && !cancelledAtCompletion) {
            for (auto* widget : QApplication::topLevelWidgets())
                if (auto* progress = qobject_cast<QProgressDialog*>(widget);
                    progress && progress->labelText().startsWith("Comparing measured")) {
                    for (auto* button : progress->findChildren<QPushButton*>())
                        if (button->text().contains("Cancel")) { cancelledAtCompletion = true; button->click(); }
                }
        }
        if (event->type() == QEvent::Show)
            if (auto* comparison = qobject_cast<ComparisonDialog*>(receiver)) {
                comparisonShown = true;
                QTimer::singleShot(0, comparison, &QDialog::reject);
            }
        return false;
    }
};

// Qt 6.8 QTRY_* macros narrow a chrono duration inside the macro expansion
// under -Wconversion. Keep the same bounded event-loop wait without narrowing.
template<class Predicate>
bool wait_until(Predicate ready, int timeout_ms) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < timeout_ms)
        QTest::qWait(10);
    return ready();
}

void put16(QByteArray& b, qint16 v) { b.append(char(v & 0xff)); b.append(char((v >> 8) & 0xff)); }
void put32(QByteArray& b, quint32 v) { for (int s = 0; s < 32; s += 8) b.append(char((v >> s) & 0xff)); }

// Stereo 16-bit WAV: 60 Hz left, 50 Hz right.
QString writeWav(const QString& dir, const QString& name, double seconds, double leftHz = 60.0, double rightHz = 50.0, int channels = 2) {
    const quint32 rate = 4096, frames = quint32(seconds * rate);
    QByteArray data;
    for (quint32 i = 0; i < frames; ++i)
        for (int c = 0; c < channels; ++c) {
            const double hz = c == 0 ? leftHz : c == 1 ? rightHz : leftHz * (c + 1);
            put16(data, qint16(std::lround((c == 0 ? 0.5 : 0.25) * 32767 * std::sin(2 * std::numbers::pi * hz * i / rate))));
        }
    QByteArray body("fmt ");
    put32(body, 16); put16(body, 1); put16(body, qint16(channels)); put32(body, rate); put32(body, rate * quint32(channels) * 2); put16(body, qint16(channels * 2)); put16(body, 16);
    body += "data"; put32(body, quint32(data.size())); body += data;
    QByteArray file("RIFF");
    put32(file, quint32(body.size() + 4));
    file += "WAVE"; file += body;
    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) qFatal("cannot create test fixture");
    f.write(file);
    return path;
}
} // namespace

class GuiTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir_;
private slots:
    void findingsAccessibilityIncludesMeasurementAndUnits() {
        FindingsModel model;
        humtrace::ui::FindingRow row;
        row.channel = 2;
        row.median_frequency_hz = 60.125;
        model.setRows({row});
        const auto accessible = model.data(model.index(0, 4), Qt::AccessibleTextRole).toString();
        QVERIFY2(accessible.contains("Median freq (Hz)") && accessible.contains("60.125"), qPrintable(accessible));
        QCOMPARE(model.headerData(4, Qt::Horizontal, Qt::AccessibleTextRole).toString(), QString("Median freq (Hz)"));
        QVERIFY(!model.headerData(-1, Qt::Horizontal, Qt::DisplayRole).isValid());
    }

    void spectrumReadoutDistinguishesDisplayWidthFromUncertainty() {
        SpectrumChart chart;
        const std::vector<float> levels{-30.0F, -40.0F};
        const std::vector<double> hz{20.0, 40.0};
        chart.setFrame(&levels, &hz, 80.0, -100.0, 0.0);
        QTest::keyClick(&chart, Qt::Key_Right);
        QVERIFY2(chart.accessibleDescription().contains("display row width"), qPrintable(chart.accessibleDescription()));
        QVERIFY(chart.accessibleDescription().contains("uncertainty is not estimated"));
        chart.setFrame(nullptr, nullptr, 0.0, -100.0, 0.0);
        QVERIFY(!chart.accessibleDescription().contains("-30.0"));
    }

    void settingsDialogExposesDetectorControls() {
        MainWindow window;
        QStringList missing;
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) { missing << "settings dialog"; return; }
            const QStringList expected{
                "Analysis frame size", "Analysis hop size", "Remove spectrum mean",
                "Minimum search frequency", "Maximum search frequency", "Minimum peak separation",
                "Local floor guard bins", "Local floor radius bins", "Harmonic search tolerance",
                "Welch segment size", "Welch hop size", "Remove Welch mean", "Relative track match tolerance"};
            for (const auto& name : expected) {
                bool found = false;
                for (auto* control : dialog->findChildren<QWidget*>())
                    found = found || control->accessibleName() == name;
                if (!found) missing << name;
            }
            dialog->reject();
        });
        QVERIFY(QMetaObject::invokeMethod(&window, "showSettings", Qt::DirectConnection));
        QVERIFY2(missing.isEmpty(), qPrintable(missing.join(", ")));
    }

    void unchangedSettingsKeepCompletedResultsCurrent() {
        MainWindow window;
        window.openRecording(writeWav(dir_.path(), "same-settings.wav", 3.0));
        window.startAnalysis();
        QVERIFY(wait_until([&] { return window.state() == humtrace::ui::JobState::Complete; }, 20000));
        window.setPipelineSettings({});
        QCOMPARE(window.state(), humtrace::ui::JobState::Complete);
        humtrace::PipelineSettings changed;
        changed.peak_search.minimum_separation_hz = 2.5;
        window.setPipelineSettings(changed);
        QCOMPARE(window.state(), humtrace::ui::JobState::Invalidated);
    }

    void settingsDialogValuesReachTheWorkerSnapshot() {
        MainWindow window;
        QStringList missing;
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) { missing << "dialog"; return; }
            const auto control = [&](const QString& name) -> QWidget* {
                for (auto* candidate : dialog->findChildren<QWidget*>())
                    if (candidate->accessibleName() == name) return candidate;
                missing << name;
                return nullptr;
            };
            const auto combo = [&](const QString& name, int value) {
                if (auto* input = qobject_cast<QComboBox*>(control(name))) input->setCurrentIndex(input->findData(value));
            };
            const auto integer = [&](const QString& name, int value) {
                if (auto* input = qobject_cast<QSpinBox*>(control(name))) input->setValue(value);
            };
            const auto real = [&](const QString& name, double value) {
                if (auto* input = qobject_cast<QDoubleSpinBox*>(control(name))) input->setValue(value);
            };
            const auto check = [&](const QString& name, bool value) {
                if (auto* input = qobject_cast<QCheckBox*>(control(name))) input->setChecked(value);
            };
            combo("Analysis frame size", 1024); integer("Analysis hop size", 256);
            check("Remove spectrum mean", false);
            real("Minimum search frequency", 40); real("Maximum search frequency", 800);
            real("Minimum peak separation", 10); integer("Local floor guard bins", 1);
            integer("Local floor radius bins", 6); real("Harmonic search tolerance", 0.75);
            combo("Welch segment size", 512); integer("Welch hop size", 256);
            check("Remove Welch mean", true); real("Relative track match tolerance", 0.005);
            dialog->accept();
        });
        QVERIFY(QMetaObject::invokeMethod(&window, "showSettings", Qt::DirectConnection));
        QVERIFY2(missing.isEmpty(), qPrintable(missing.join(", ")));
        window.openRecording(writeWav(dir_.path(), "dialog-snapshot.wav", 3.0));
        window.startAnalysis();
        QVERIFY(wait_until([&] { return window.state() == humtrace::ui::JobState::Complete; }, 20000));
        const auto& report = *window.report();
        QCOMPARE(report.frame_size_samples, std::size_t{1024}); QCOMPARE(report.hop_size_samples, std::size_t{256});
        QVERIFY(!report.spectrum_remove_mean); QVERIFY(report.welch.remove_mean);
        QCOMPARE(report.peak_search.minimum_frequency_hz, 40.0); QCOMPARE(report.peak_search.maximum_frequency_hz, 800.0);
        QCOMPARE(report.peak_search.minimum_separation_hz, 10.0);
        QCOMPARE(report.peak_search.floor_guard_bins, std::size_t{1}); QCOMPARE(report.peak_search.floor_radius_bins, std::size_t{6});
        QCOMPARE(report.harmonic_tolerance_hz, 0.75); QCOMPARE(report.tracker.relative_tolerance, 0.005);
        QCOMPARE(report.welch.segment_size, std::size_t{512}); QCOMPARE(report.welch.hop_size, std::size_t{256});
    }

    void customFrameAndHopChoosePagedStorageFromActualWindowCount() {
        MainWindow window;
        humtrace::PipelineSettings settings;
        settings.frame_size_samples = 16;
        settings.hop_size_samples = 16;
        settings.welch.segment_size = 256;
        settings.welch.hop_size = 128;
        // A valid peak cap makes 128 small windows cross the retention estimate,
        // while the automatic single-window analysis remains retained. Only actual
        // peaks are materialized, so this keeps spool I/O bounded on native runners.
        settings.max_peaks_per_frame = 256;
        settings.spectrogram_rows = 8;
        const auto path = writeWav(dir_.path(), "custom-paging.wav", 0.5, 60.0, 50.0, 1);
        const auto expected = humtrace::analyze_file(path.toStdString(), settings, {}, true);
        QCOMPARE(expected.report.channel_results[0].frames.size(), std::size_t{128});
        auto automatic = settings;
        automatic.frame_size_samples = 0;
        automatic.hop_size_samples = 0;
        window.setPipelineSettings(automatic);
        window.openRecording(path);
        QString error;
        window.setErrorHandler([&](const QString&, const QString& message) { error = message; });
        window.startAnalysis();
        QVERIFY(wait_until([&] { return !humtrace::ui::is_active(window.state()); }, 20000));
        QVERIFY2(window.state() == humtrace::ui::JobState::Complete, qPrintable(error));
        QVERIFY(!window.hasPagedResults());
        QCOMPARE(window.report()->channel_results[0].frames.size(), std::size_t{1});

        window.setPipelineSettings(settings);
        window.startAnalysis();
        QVERIFY(wait_until([&] { return !humtrace::ui::is_active(window.state()); }, 20000));
        QVERIFY2(window.state() == humtrace::ui::JobState::Complete, qPrintable(error));
        QVERIFY(window.hasPagedResults());
        QCOMPARE(window.report()->frame_size_samples, std::size_t{16});
        QCOMPARE(window.report()->hop_size_samples, std::size_t{16});
        const auto output = dir_.filePath("custom-paging-export.json");
        QVERIFY(window.exportReport(output, false).isEmpty());
        QFile exported(output);
        QVERIFY(exported.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(exported.readAll()),
            QJsonDocument::fromJson(QByteArray::fromStdString(humtrace::serialize_report_json(expected.report))));
    }

    void spectrumTableIncludesEveryDisplayedRowWithAccessibleUnits() {
        const auto path = writeWav(dir_.path(), "table-values.wav", 3.0);
        MainWindow window;
        window.openRecording(path);
        window.startAnalysis();
        QVERIFY(wait_until([&] { return window.state() == humtrace::ui::JobState::Complete; }, 20000));
        auto* table = window.findChild<QTableWidget*>();
        QVERIFY(table);
        auto* accessible = QAccessible::queryAccessibleInterface(table);
        QVERIFY(accessible && accessible->tableInterface());
        int plotRows = 0;
        for (int r = 0; r < table->rowCount(); ++r) {
            if (!table->item(r, 0)->text().startsWith("Display row ")) continue;
            ++plotRows;
            const auto text = table->item(r, 2)->data(Qt::AccessibleTextRole).toString();
            QVERIFY2(text.contains("dBFS") && text.contains("maximum FFT bin"), qPrintable(text));
        }
        QCOMPARE(plotRows, 128);
    }

    void waveformTablePreservesEnvelopeValuesAndPartialBoundsForBothStorageModes() {
        const auto path = writeWav(dir_.path(), "waveform-table.wav", 3.017);
        humtrace::PipelineSettings settings;
        settings.waveform_buckets = 17;
        const auto plain = humtrace::analyze_file(path.toStdString(), settings, {}, true);
        const auto totalSamples = plain.report.channel_results[0].frames.back().start_sample + plain.report.frame_size_samples;
        for (bool paged : {false, true}) {
            if (!QFile::exists(path)) writeWav(dir_.path(), "waveform-table.wav", 3.017);
            MainWindow window;
            window.setPipelineSettings(settings);
            window.setPagedAnalysis(paged);
            QTableView* table = nullptr;
            for (auto* candidate : window.findChildren<QTableView*>())
                if (candidate->accessibleName() == "Waveform envelope table") table = candidate;
            QVERIFY2(table, "waveform envelope has no table alternative");
            QVERIFY(!qobject_cast<QTableWidget*>(table));
            QCOMPARE(table->model()->rowCount(), 0);
            window.openRecording(path);
            QCOMPARE(table->model()->rowCount(), 0); // Ready must not retain old measurements.
            window.startAnalysis();
            QVERIFY(wait_until([&] { return window.state() == humtrace::ui::JobState::Complete; }, 20000));
            QVERIFY(QFile::remove(path)); // Channel changes and bounds use stored results, never reopen input.
            auto* model = table->model();
            QVERIFY(model->inherits("QAbstractTableModel"));
            QVERIFY(QAccessible::queryAccessibleInterface(table)->tableInterface());
            for (int channel = 0; channel < 2; ++channel) {
                window.viewState()->setChannel(channel);
                const auto& expected = plain.views[static_cast<std::size_t>(channel)];
                QCOMPARE(model->rowCount(), int(expected.waveform_min.size()));
                QCOMPARE(model->columnCount(), 7);
                for (int row = 0; row < model->rowCount(); ++row) {
                    const auto i = static_cast<std::size_t>(row);
                    const auto start = static_cast<std::uint64_t>(row) * expected.waveform_samples_per_bucket;
                    const auto end = std::min(totalSamples, start + expected.waveform_samples_per_bucket);
                    const auto value = [&](int column) { return model->data(model->index(row, column), Qt::UserRole); };
                    QCOMPARE(value(0).toInt(), channel + 1);
                    QCOMPARE(value(1).toULongLong(), start); QCOMPARE(value(2).toULongLong(), end);
                    QCOMPARE(value(3).toDouble(), double(start) / plain.report.sample_rate_hz);
                    QCOMPARE(value(4).toDouble(), double(end) / plain.report.sample_rate_hz);
                    QCOMPARE(value(5).toDouble(), double(expected.waveform_min[i]));
                    QCOMPARE(value(6).toDouble(), double(expected.waveform_max[i]));
                }
                const auto last = model->rowCount() - 1;
                QCOMPARE(model->data(model->index(last, 2), Qt::UserRole).toULongLong(), totalSamples);
                QVERIFY(totalSamples % expected.waveform_samples_per_bucket != 0);
                const auto accessible = model->data(model->index(last, 5), Qt::AccessibleTextRole).toString();
                QVERIFY2(accessible.contains("normalized FS") && accessible.contains(QString("channel %1").arg(channel + 1)), qPrintable(accessible));
                QVERIFY(model->headerData(2, Qt::Horizontal, Qt::AccessibleTextRole).toString().contains("exclusive"));
            }
            if (const char* shots = std::getenv("HUMTRACE_SCREENSHOT_DIR")) {
                for (auto* tabs : window.findChildren<QTabWidget*>())
                    if (tabs->indexOf(table) >= 0) tabs->setCurrentWidget(table);
                window.resize(1024, 680);
                window.show();
                QTest::qWait(30);
                const auto mode = paged ? "paged" : "retained";
                window.grab().save(QString("%1/waveform-table-%2-bounds.png").arg(shots, mode));
                table->horizontalScrollBar()->setValue(table->horizontalScrollBar()->maximum());
                QTest::qWait(20);
                window.grab().save(QString("%1/waveform-table-%2-levels.png").arg(shots, mode));
            }
            window.openRecording(writeWav(dir_.path(), "waveform-table-next.wav", 2.0));
            QCOMPARE(model->rowCount(), 0);
        }
    }

    void waveformTableUsesVirtualRowsForAMillionExistingBuckets() {
        constexpr std::size_t count = 1U << 20;
        humtrace::ChannelView source;
        source.waveform_min.assign(count, -0.125F);
        source.waveform_max.assign(count, 0.5F);
        source.waveform_samples_per_bucket = 7;
        const auto residentBytes = []() -> std::uint64_t {
            QFile status("/proc/self/smaps_rollup");
            if (!status.open(QIODevice::ReadOnly)) return 0; // RSS accounting is Linux-specific.
            for (const auto& line : status.readAll().split('\n'))
                if (line.startsWith("Rss:")) return line.simplified().split(' ')[1].toULongLong() * 1024;
            return 0;
        };
        const auto before = residentBytes();
        WaveformModel model;
        QTableView table;
        table.verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        table.horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        table.setModel(&model);
        const auto samples = std::uint64_t{7} * count - 3;
        model.setEnvelope(&source, 48000, samples, 2);
        QCOMPARE(model.rowCount(), int(count));
        QCOMPARE(model.columnCount(), 7);
        const auto last = int(count) - 1;
        QCOMPARE(model.data(model.index(last, 2), Qt::UserRole).toULongLong(), samples);
        QCOMPARE(model.data(model.index(last, 5), Qt::UserRole).toDouble(), -0.125);
        QCOMPARE(model.data(model.index(last, 6), Qt::UserRole).toDouble(), 0.5);
        QVERIFY(model.data(model.index(last, 5), Qt::AccessibleDescriptionRole).toString().contains("not individual raw audio"));
        const auto after = residentBytes();
        if (before && after) {
            QVERIFY2(after <= before + 64U * 1024U * 1024U, "virtual envelope table allocated more than 64 MiB beyond its existing summary vectors");
            qInfo("WAVEFORM_VIRTUAL rows=%d rss_growth_bytes=%llu", int(count), static_cast<unsigned long long>(after > before ? after - before : 0));
        }
        QVERIFY(!model.headerData(-1, Qt::Horizontal, Qt::DisplayRole).isValid());
        model.setEnvelope(nullptr, 0, 0, 0);
        QCOMPARE(model.rowCount(), 0);
    }

    void silentFramesDoNotInventAnInterpolatedPeak() {
        MainWindow window;
        humtrace::PipelineSettings settings;
        settings.spectrum_remove_mean = false;
        window.setPipelineSettings(settings);
        window.openRecording(writeWav(dir_.path(), "silent-frame.wav", 3.0, 0.0, 0.0, 1));
        window.startAnalysis();
        QVERIFY(wait_until([&] { return window.state() == humtrace::ui::JobState::Complete; }, 20000));
        auto* table = window.findChild<QTableWidget*>();
        QVERIFY(table && table->rowCount());
        QVERIFY2(table->item(0, 0)->text().contains("no non-DC peak"), qPrintable(table->item(0, 0)->text()));
        QCOMPARE(table->item(0, 1)->text(), QString("unavailable"));
        bool foundMethod = false;
        for (auto* label : window.findChildren<QLabel*>())
            if (label->accessibleName() == "Spectrum method and resolution") {
                foundMethod = true;
                QVERIFY(label->text().contains("frame mean retained"));
                QVERIFY(label->text().contains("uncertainty is not estimated"));
            }
        QVERIFY(foundMethod);
    }

    void recordingHeaderRefreshesDigestAndClearsPreviousMethod() {
        MainWindow window;
        const auto path = writeWav(dir_.path(), "refresh-header.wav", 3.0);
        window.openRecording(path);
        window.startAnalysis();
        QVERIFY(wait_until([&] { return window.state() == humtrace::ui::JobState::Complete; }, 20000));
        const auto firstDigest = window.report()->input_sha256;
        writeWav(dir_.path(), "refresh-header.wav", 3.0, 120.0, 100.0);
        window.startAnalysis();
        QVERIFY(wait_until([&] { return window.state() == humtrace::ui::JobState::Complete; }, 20000));
        QVERIFY(firstDigest != window.report()->input_sha256);
        for (auto* label : window.findChildren<QLabel*>())
            if (label->accessibleName() == "Recording identity") {
                QVERIFY2(label->text().contains(QString::fromStdString(window.report()->input_sha256).left(16)), qPrintable(label->text()));
                QCOMPARE(label->accessibleDescription(), label->text());
            }
        window.openRecording(writeWav(dir_.path(), "new-method.wav", 2.0));
        for (auto* label : window.findChildren<QLabel*>())
            if (label->accessibleName() == "Spectrum method and resolution") QVERIFY(label->text().isEmpty());
    }

    void comparisonPagesPreserveBoundaryRows() {
        humtrace::ComparisonReport report;
        for (std::size_t i = 0; i < 513; ++i) {
            humtrace::TrackRef value;
            value.median_frequency_hz = static_cast<double>(i);
            report.unmatched_a.push_back(value);
        }
        ComparisonDialog dialog(report);
        auto* table = dialog.findChild<QTableWidget*>();
        QPushButton* next = nullptr;
        QPushButton* previous = nullptr;
        for (auto* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == "Next rows") next = button;
            if (button->text() == "Previous rows") previous = button;
        }
        QVERIFY(table && next && previous);
        QVERIFY(table->item(0, 4)->data(Qt::AccessibleTextRole).toString().contains("unavailable"));
        QVERIFY(QAccessible::queryAccessibleInterface(table)->tableInterface());
        QCOMPARE(table->rowCount(), 256);
        QVERIFY(!previous->isEnabled());
        next->click();
        QCOMPARE(table->item(0, 2)->text(), QString("256.000"));
        next->click();
        QCOMPARE(table->rowCount(), 1);
        QCOMPARE(table->item(0, 2)->text(), QString("512.000"));
        QVERIFY(!next->isEnabled());
        previous->click();
        QCOMPARE(table->rowCount(), 256);
        QCOMPARE(table->item(255, 2)->text(), QString("511.000"));
    }

    void comparisonCancelAndCloseJoinWorkersAndKeepTheOriginalReport() {
        const auto original = writeWav(dir_.path(), "comparison-a.wav", 3.0);
        const auto other = writeWav(dir_.path(), "comparison-b.wav", 1500.0);
        MainWindow window;
        window.openRecording(original);
        window.startAnalysis();
        QVERIFY(wait_until([&] { return window.state() == humtrace::ui::JobState::Complete; }, 20000));
        const auto digest = window.report()->input_sha256;
        QString error;
        window.setErrorHandler([&](const QString&, const QString& message) { error = message; });
        for (bool closeWindow : {false, true}) {
            bool opened = false, stopped = false, timedOut = false;
            QElapsedTimer deadline; deadline.start();
            QTimer driver;
            connect(&driver, &QTimer::timeout, [&] {
                if (auto* chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
                    chooser->selectFile(other);
                    opened = true;
                    QMetaObject::invokeMethod(chooser, "accept", Qt::DirectConnection);
                } else if (auto* progress = window.findChild<QProgressDialog*>(); progress && opened && !stopped) {
                    stopped = true;
                    if (closeWindow) window.close();
                    else for (auto* button : progress->findChildren<QPushButton*>())
                        if (button->text().contains("Cancel")) button->click();
                }
                if (deadline.elapsed() > 15000) {
                    timedOut = true;
                    if (auto* modal = qobject_cast<QDialog*>(QApplication::activeModalWidget())) modal->reject();
                }
            });
            driver.start(1);
            QVERIFY(QMetaObject::invokeMethod(&window, "chooseAndCompare", Qt::DirectConnection));
            driver.stop();
            QVERIFY(opened && stopped && !timedOut);
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QCOMPARE(window.state(), humtrace::ui::JobState::Complete);
            QCOMPARE(window.report()->input_sha256, digest);
            QVERIFY(!window.findChild<QProgressDialog*>());
        }
    }

    void lateComparisonCancelDoesNotOpenResults() {
        const auto other = writeWav(dir_.path(), "late-comparison-b.wav", 4.0);
        MainWindow window;
        window.show();
        window.openRecording(writeWav(dir_.path(), "late-comparison-a.wav", 4.0));
        window.startAnalysis();
        QVERIFY(wait_until([&] { return window.state() == humtrace::ui::JobState::Complete; }, 20000));
        for (bool cancel : {false, true}) {
            CancelAtComparisonCompletion filter;
            filter.cancelOnCompletion = cancel;
            qApp->installEventFilter(&filter);
            QTimer chooser;
            connect(&chooser, &QTimer::timeout, [&] {
                if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
                    dialog->selectFile(other);
                    QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                }
            });
            chooser.start(1);
            const bool invoked = QMetaObject::invokeMethod(&window, "chooseAndCompare", Qt::DirectConnection);
            chooser.stop();
            qApp->removeEventFilter(&filter);
            QVERIFY(invoked);
            QCOMPARE(filter.cancelledAtCompletion, cancel);
            QCOMPARE(filter.comparisonShown, !cancel);
        }
    }
    void initTestCase() { QVERIFY(dir_.isValid()); }

    void startsEmptyWithDisabledActions() {
        MainWindow w;
        QCOMPARE(w.state(), humtrace::ui::JobState::Empty);
        QVERIFY(w.report() == nullptr);
    }

    void minimumWindowSizeKeepsEssentialControlsVisible_data() {
        QTest::addColumn<bool>("largerControlFonts");
        QTest::newRow("platform defaults") << false;
        QTest::newRow("larger native control fonts") << true;
    }

    void minimumWindowSizeKeepsEssentialControlsVisible() {
        QFETCH(bool, largerControlFonts);
        MainWindow window;
        if (largerControlFonts)
            for (auto* control : window.findChildren<QWidget*>()) {
                const auto name = control->accessibleName();
                if (name != "Spectrogram frequency scale" && name != "Spectrogram palette" &&
                    name != "Spectrogram floor" && name != "Spectrogram ceiling") continue;
                auto font = control->font();
                font.setPointSizeF(font.pointSizeF() * 1.3);
                control->setFont(font);
            }
        window.resize(1024, 680);
        window.show();
        window.openRecording(writeWav(dir_.path(), "minimum-window.wav", 3.0));
        const auto check = [&] {
            QCOMPARE(window.size(), QSize(1024, 680));
            for (auto* button : window.findChildren<QToolButton*>()) {
                if (button->text().isEmpty()) continue;
                QVERIFY2(button->isVisibleTo(&window), qPrintable(button->text()));
                QVERIFY2(button->width() >= button->sizeHint().width(), qPrintable(button->text()));
            }
            for (auto* control : window.findChildren<QWidget*>()) {
                const auto name = control->accessibleName();
                if (name != "Spectrogram frequency scale" && name != "Spectrogram palette" &&
                    name != "Spectrogram floor" && name != "Spectrogram ceiling") continue;
                const QRect visible(control->mapTo(&window, QPoint{}), control->size());
                QVERIFY2(window.rect().contains(visible) && control->isVisibleTo(&window), qPrintable(name));
                QVERIFY(control->width() >= control->minimumSizeHint().width());
            }
        };
        QTest::qWait(20);
        check();
        window.startAnalysis();
        QVERIFY(wait_until([&] { return window.state() == humtrace::ui::JobState::Complete; }, 20000));
        QTest::qWait(20);
        check();
    }

    void analyzesStereoFileAndShowsPerChannelFindings() {
        const auto path = writeWav(dir_.path(), "stereo.wav", 4.0);
        MainWindow w;
        w.show();
        w.openRecording(path);
        QCOMPARE(w.state(), humtrace::ui::JobState::Ready);
        w.startAnalysis();
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 20000));
        QVERIFY(w.report() != nullptr);
        QCOMPARE(int(w.report()->channels), 2);
        QCOMPARE(w.findingsModel()->rowCount(), 2);
        // Channels are never mixed: ch1 ~60 Hz, ch2 ~50 Hz.
        QVERIFY(std::abs(w.findingsModel()->row(0).median_frequency_hz - 60.0) < 0.3);
        QVERIFY(std::abs(w.findingsModel()->row(1).median_frequency_hz - 50.0) < 0.3);
        // The recorded digest equals the file's actual SHA-256 (source preserved).
        std::ifstream in(path.toStdString(), std::ios::binary);
        QCOMPARE(QString::fromStdString(w.report()->input_sha256), QString::fromStdString(humtrace::sha256_hex(in)));
        if (const char* shots = std::getenv("HUMTRACE_SCREENSHOT_DIR")) {
            w.resize(1440, 900);
            QTest::qWait(200);
            w.grab().save(QString("%1/complete.png").arg(shots));
            w.resize(1024, 680);
            QTest::qWait(200);
            w.grab().save(QString("%1/narrow.png").arg(shots));
        }
    }

    void visualStatesRenderWithoutErrors() {
        // Renders every display state; images are saved when HUMTRACE_SCREENSHOT_DIR is set.
        const auto path = writeWav(dir_.path(), "vis.wav", 6.0, 60.0, 180.0);
        MainWindow w;
        w.show();
        w.openRecording(path);
        const char* shots = std::getenv("HUMTRACE_SCREENSHOT_DIR");
        auto shot = [&](const char* name) { QTest::qWait(50); const auto img = w.grab(); QVERIFY(!img.isNull()); if (shots) img.save(QString("%1/%2.png").arg(shots, name)); };
        shot("ready");
        w.startAnalysis();
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 20000));
        w.setDisplayOptions(1, 1, -90, -20);
        shot("linear-grayscale");
        w.setDisplayOptions(0, 0, -100, -10);
        w.viewState()->setChannel(1);
        w.viewState()->zoomAround(3.0, 0.25);
        shot("zoomed-channel2");
        humtrace::PipelineSettings changed;
        changed.support_threshold_dbfs = -30;
        w.setPipelineSettings(changed);
        QCOMPARE(w.state(), humtrace::ui::JobState::Invalidated);
        shot("stale");
        w.setPipelineSettings({});
        w.startAnalysis();
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 20000));
        w.resize(1024, 680);
        shot("narrow");
        QString msg;
        w.setErrorHandler([&](const QString&, const QString& m) { msg = m; });
        w.openRecording(dir_.path() + "/does-not-exist.wav");
        QVERIFY(!msg.isEmpty());
        shot("error-state");
    }

    void roundTwoRegressions() {
        // 1. Selecting a finding shows the track's FIRST observed frame, not the frame before it.
        const auto path = writeWav(dir_.path(), "late.wav", 12.0, 60.0, 50.0, 1);
        MainWindow w;
        w.show();
        w.openRecording(path);
        humtrace::PipelineSettings s;
        w.setPipelineSettings(s);
        w.startAnalysis();
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 20000));
        w.findingsTable()->selectRow(0);
        QTableWidget* frameTable = nullptr;
        for (auto* t : w.findChildren<QTableWidget*>()) if (t->accessibleName().contains("Spectrum measurement table")) frameTable = t;
        QVERIFY(frameTable && frameTable->rowCount() > 0);
        const auto first = w.findingsModel()->row(0).first_frame;
        QVERIFY2(frameTable->item(0, 0)->text().startsWith(QString("Frame %1 ").arg(first)), qPrintable(frameTable->item(0, 0)->text()));
        // 2. Floor above ceiling is normalized for every view.
        w.setDisplayOptions(0, 0, -50, -80);
        QDoubleSpinBox *floorBox = nullptr, *ceilBox = nullptr;
        for (auto* b : w.findChildren<QDoubleSpinBox*>()) {
            if (b->accessibleName() == "Spectrogram floor") floorBox = b;
            if (b->accessibleName() == "Spectrogram ceiling") ceilBox = b;
        }
        QVERIFY(floorBox && ceilBox && floorBox->value() < ceilBox->value());
        w.grab();
        // 3. Channel selection survives re-analysis (stereo file).
        const auto stereo = writeWav(dir_.path(), "keepch.wav", 4.0);
        w.openRecording(stereo);
        w.startAnalysis();
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 20000));
        w.viewState()->setChannel(1);
        w.startAnalysis();
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 20000));
        QCOMPARE(w.viewState()->channel(), 1);
        // 4. Busy: opening another file during a run is refused and the run is unaffected.
        w.startAnalysis();
        w.openRecording(path);
        QVERIFY(humtrace::ui::is_active(w.state()) || w.state() == humtrace::ui::JobState::Complete);
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 20000));
        QCOMPARE(w.report()->channels, quint16(2));
    }

    void manyChannelsAndLongNamesStayUsable() {
        const QString longName = QString("n").repeated(200) + ".wav";
        const auto path = writeWav(dir_.path(), longName, 2.0, 60.0, 50.0, 32);
        MainWindow w;
        w.show();
        w.resize(1440, 900);
        w.openRecording(path);
        QVERIFY2(w.minimumSizeHint().width() < 1500, qPrintable(QString::number(w.minimumSizeHint().width())));
        w.startAnalysis();
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 30000));
        QCOMPARE(int(w.report()->channels), 32);
        const auto img = w.grab(); // 32 lanes must fall back to one readable lane without crashing
        QVERIFY(!img.isNull());
        QVERIFY(w.minimumSizeHint().width() < 1500);
    }

    void cancelKeepsCancellingFeedback() {
        const auto path = writeWav(dir_.path(), "cancelfb.wav", 1500.0);
        MainWindow w;
        w.show();
        w.openRecording(path);
        w.startAnalysis();
        QTest::qWait(30);
        QMetaObject::invokeMethod(&w, "cancelAnalysis");
        QLabel* state = nullptr;
        for (auto* l : w.findChildren<QLabel*>()) if (l->accessibleName() == "Job state") state = l;
        QVERIFY(state);
        for (int i = 0; i < 40 && humtrace::ui::is_active(w.state()); ++i) {
            QTest::qWait(5);
            if (w.state() == humtrace::ui::JobState::Cancelling) QVERIFY2(!state->text().startsWith("Analyzing") && !state->text().startsWith("Hashing"), qPrintable(state->text()));
        }
    }

    void selectingFindingSynchronizesViewAndInspector() {
        const auto path = writeWav(dir_.path(), "sel.wav", 4.0);
        MainWindow w;
        w.show();
        w.openRecording(path);
        w.startAnalysis();
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 20000));
        w.findingsTable()->selectRow(1);
        QCOMPARE(w.viewState()->channel(), 1);
        QVERIFY(w.viewState()->hasSelection());
        QVERIFY(w.viewState()->selectionEnd() > w.viewState()->selectionStart());
    }

    void keyboardOnlyNavigationMovesCursorAndZoomsWithoutReanalysis() {
        const auto path = writeWav(dir_.path(), "kbd.wav", 4.0);
        MainWindow w;
        w.show();
        w.openRecording(path);
        w.startAnalysis();
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 20000));
        const auto digest = w.report()->input_sha256;
        const auto before = w.report()->channel_results[0].frames.size();
        auto* wave = w.findChild<WaveformChart*>();
        QVERIFY(wave);
        wave->setFocus();
        const double c0 = w.viewState()->cursor();
        QTest::keyClick(wave, Qt::Key_Right);
        QVERIFY(w.viewState()->cursor() > c0);
        QTest::keyClick(wave, Qt::Key_Left);
        QCOMPARE(w.viewState()->cursor(), c0);
        const double len0 = w.viewState()->view().length();
        QTest::keyClick(wave, Qt::Key_Plus);
        QVERIFY(w.viewState()->view().length() < len0);
        QTest::keyClick(wave, Qt::Key_0);
        QCOMPARE(w.viewState()->view().length(), len0);
        QTest::keyClick(wave, Qt::Key_Right, Qt::ShiftModifier);
        QVERIFY(w.viewState()->hasSelection());
        QTest::keyClick(wave, Qt::Key_Escape);
        QVERIFY(!w.viewState()->hasSelection());
        // View-only changes never touch analysis results.
        QCOMPARE(w.report()->input_sha256, digest);
        QCOMPARE(w.report()->channel_results[0].frames.size(), before);
    }

    void cancelStopsAnalysisAndDiscardsPartialResults() {
        const auto path = writeWav(dir_.path(), "long.wav", 1500.0); // ~24 MB
        MainWindow w;
        w.show();
        w.openRecording(path);
        w.startAnalysis();
        QTest::qWait(30);
        QElapsedTimer t;
        t.start();
        QMetaObject::invokeMethod(&w, "cancelAnalysis");
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Cancelled || w.state() == humtrace::ui::JobState::Complete; }, 20000));
        if (w.state() == humtrace::ui::JobState::Cancelled) {
            QVERIFY2(t.elapsed() < 3000, "cancellation latency");
            QVERIFY(w.report() == nullptr);
        }
        // Source remains untouched and a new run is possible.
        QVERIFY(QFile::exists(path));
    }

    void invalidFileFailsWithoutModifyingIt() {
        const QString bad = dir_.path() + "/bad.wav";
        QFile f(bad);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("this is not audio");
        f.close();
        MainWindow w;
        QString stage, message;
        w.setErrorHandler([&](const QString& s, const QString& m) { stage = s; message = m; });
        w.openRecording(bad);
        QCOMPARE(w.state(), humtrace::ui::JobState::Empty); // nothing was opened
        QVERIFY(message.contains("RIFF"));
        QFile check(bad);
        QVERIFY(check.open(QIODevice::ReadOnly));
        QCOMPARE(check.readAll(), QByteArray("this is not audio"));
    }

    void badOpenKeepsCurrentResultsAndNamesWithPercentSignsAreLiteral() {
        const auto path = writeWav(dir_.path(), "a%2b&amp;x.wav", 3.0);
        MainWindow w;
        w.show();
        w.openRecording(path);
        w.startAnalysis();
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 20000));
        QLabel* header = nullptr;
        for (auto* l : w.findChildren<QLabel*>()) if (l->accessibleName() == "Recording identity") header = l;
        QVERIFY(header);
        QVERIFY2(header->text().contains("a%2b&amp;x.wav"), qPrintable(header->text()));
        QCOMPARE(header->textFormat(), Qt::PlainText);
        // A missing file or a non-WAV must not discard the finished analysis.
        QString shown;
        w.setErrorHandler([&](const QString&, const QString& m) { shown = m; });
        w.openRecording(dir_.path() + "/missing.wav");
        QVERIFY(!shown.isEmpty());
        QCOMPARE(w.state(), humtrace::ui::JobState::Complete);
        QVERIFY(w.report() != nullptr);
        QVERIFY(w.findingsModel()->rowCount() > 0);
        // Export may not overwrite the recording itself.
        QVERIFY(!w.exportReport(path, true).isEmpty());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(f.read(4) == "RIFF");
    }

    void settingsChangeMarksResultsStaleAndExportRedactsPath() {
        const auto path = writeWav(dir_.path(), "export.wav", 3.0);
        MainWindow w;
        w.show();
        w.openRecording(path);
        w.startAnalysis();
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 20000));
        const QString out = dir_.path() + "/report.json";
        QVERIFY(w.exportReport(out, true).isEmpty());
        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto json = f.readAll();
        QVERIFY(json.contains("\"path\": \"export.wav\""));
        QVERIFY(!json.contains(dir_.path().toUtf8()));
        QCOMPARE(QJsonDocument::fromJson(json).object()["schema_version"].toString(), QString::fromLatin1(humtrace::kReportSchemaVersion));
        QVERIFY(QDir(dir_.path()).entryList({"*.humtrace-tmp*"}).isEmpty());
        // Unwritable destination reports an error and leaves no temp file.
        QVERIFY(!w.exportReport(dir_.path() + "/missing-dir/report.json", false).isEmpty());
    }
    void streamsReportInBackgroundAndHandlesQuotaAndClose() {
        const auto path = writeWav(dir_.path(), "stream-desktop.wav", 30.0);
        const auto output = dir_.path() + "/stream-desktop.json";
        MainWindow w;
        w.show();
        w.openRecording(path);
        QString error;
        w.setErrorHandler([&](const QString&, const QString& message) { error = message; });
        int heartbeats = 0;
        QTimer heartbeat;
        connect(&heartbeat, &QTimer::timeout, this, [&] { ++heartbeats; });
        heartbeat.start(1);
        w.startStreamingExport(output);
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete; }, 20000));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(heartbeats > 0);
        QVERIFY(w.report() == nullptr); // full timeline was never loaded for visual preview
        QFile report(output);
        QVERIFY(report.open(QIODevice::ReadOnly));
        const auto document = QJsonDocument::fromJson(report.readAll());
        QVERIFY(document.isObject());
        QCOMPARE(document.object()["input"].toObject()["path"].toString(), QString("stream-desktop.wav"));
        report.close();
        humtrace::StreamingLimits tight;
        tight.max_resident_bytes = 1;
        w.setStreamingLimits(tight);
        const auto refused = dir_.path() + "/stream-refused.json";
        w.startStreamingExport(refused);
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Failed; }, 20000));
        QVERIFY(error.contains("memory budget"));
        QVERIFY(!QFile::exists(refused));
        w.setStreamingLimits({});
        {
            MainWindow closing;
            closing.openRecording(path);
            closing.startStreamingExport(dir_.path() + "/stream-close.json");
            QElapsedTimer timer;
            timer.start();
            closing.close();
            QVERIFY(timer.elapsed() < 250);
        }
        QVERIFY(!QFile::exists(dir_.path() + "/stream-close.json"));
        QVERIFY(QDir(dir_.path()).entryList({".humtrace-stream-*", "*.humtrace-tmp-*"}, QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
    }

    void pagedAnalysisPreservesInspectorAndBoundsCaches() {
        const auto path = writeWav(dir_.path(), "paged-desktop.wav", 30.25);
        const auto plain = humtrace::analyze_file(path.toStdString(), {}, {}, true);
        const auto expectedRows = humtrace::ui::build_findings(plain.report);
        MainWindow w;
        w.setPagedAnalysis(true);
        w.show();
        QString error;
        w.setErrorHandler([&](const QString&, const QString& message) { error = message; });
        w.openRecording(path);
        w.startAnalysis();
        QVERIFY(wait_until([&] { return w.state() == humtrace::ui::JobState::Complete || w.state() == humtrace::ui::JobState::Failed; }, 20000));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(w.hasPagedResults());
        QVERIFY(w.report()->channel_results[0].frames.empty());
        QCOMPARE(w.findingsModel()->rowCount(), 1); // paged by channel, with explicit totals
        auto* inspector = w.findChild<QTextBrowser*>();
        QVERIFY(inspector);
        w.findingsTable()->selectRow(0);
        QCOMPARE(inspector->toPlainText(), QString::fromStdString(humtrace::ui::describe_finding(plain.report, expectedRows[0])));
        w.viewState()->setChannel(1);
        QCOMPARE(w.findingsModel()->rowCount(), 1);
        w.findingsTable()->selectRow(0);
        QCOMPARE(inspector->toPlainText(), QString::fromStdString(humtrace::ui::describe_finding(plain.report, expectedRows[1])));
        auto* spectrogram = w.findChild<SpectrogramChart*>();
        QVERIFY(spectrogram);
        for (int i = 0; i < 40; ++i) {
            w.viewState()->setCursor((i % 30) + 0.25);
            w.viewState()->setView({double(i % 20), double(i % 20) + 5.0});
        }
        QVERIFY(w.pagedCacheBytes() <= 6 * 1024 * 1024);
        QVERIFY(spectrogram->imageBytes() <= 2 * 1024 * 1024);
        const auto exported = dir_.path() + "/paged-export.json";
        QVERIFY(w.exportReport(exported, false).isEmpty());
        QFile file(exported);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto actual = QJsonDocument::fromJson(file.readAll());
        const auto expected = QJsonDocument::fromJson(QByteArray::fromStdString(humtrace::serialize_report_json(plain.report)));
        QCOMPARE(actual, expected);
        // A tiny cache must evict and reload while preserving exact values.
        const auto stored = humtrace::analyze_file_paged(path.toStdString(), {});
        PagedData cache(stored, 65536);
        for (std::size_t c = 0; c < 2; ++c) for (std::uint64_t f = 0; f < stored->frame_count(c); ++f) {
            QCOMPARE(cache.frame(c, f).start_sample, plain.report.channel_results[c].frames[static_cast<std::size_t>(f)].start_sample);
            QCOMPARE(cache.row(c, f), plain.views[c].spectrogram_dbfs[static_cast<std::size_t>(f)]);
            QVERIFY(cache.cachedBytesUpperBound() <= 65536);
        }
        QCOMPARE(cache.frame(0, 0).start_sample, std::uint64_t{0});
    }

    void narrowSearchBandPreservesBothHarmonicDenominatorsInPagedInspector() {
        const auto path = writeWav(dir_.path(), "band-support.wav", 4.0, 120.0, 120.0, 1);
        humtrace::PipelineSettings settings;
        settings.peak_search.minimum_frequency_hz = 110;
        settings.peak_search.maximum_frequency_hz = 130;
        const auto plain = humtrace::analyze_file(path.toStdString(), settings, {}, true);
        const auto rows = humtrace::ui::build_findings(plain.report);
        QVERIFY(!rows.empty());
        const auto expected = QString::fromStdString(humtrace::ui::describe_finding(plain.report, rows[0]));
        QVERIFY(expected.contains("50 Hz harmonic bins above threshold: 0 to 0 of 0"));
        QVERIFY(expected.contains("60 Hz harmonic bins above threshold: 1 to 1 of 1"));
        MainWindow window;
        window.setPipelineSettings(settings);
        window.setPagedAnalysis(true);
        window.openRecording(path);
        window.startAnalysis();
        QVERIFY(wait_until([&] { return window.state() == humtrace::ui::JobState::Complete; }, 20000));
        window.findingsTable()->selectRow(0);
        QCOMPARE(window.findChild<QTextBrowser*>()->toPlainText(), expected);
    }

};

QTEST_MAIN(GuiTest)
#include "test_gui.moc"
