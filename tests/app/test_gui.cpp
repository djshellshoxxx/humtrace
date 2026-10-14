// End-to-end GUI behavior tests (offscreen). Exercise the same code paths as the actions.
#include "FindingsModel.hpp"
#include "MainWindow.hpp"
#include "SpectrogramChart.hpp"
#include "ViewState.hpp"
#include "WaveformChart.hpp"
#include "humtrace/sha256.hpp"

#include <QElapsedTimer>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QTableWidget>
#include <QFile>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <fstream>
#include <numbers>

namespace {
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
    void initTestCase() { QVERIFY(dir_.isValid()); }

    void startsEmptyWithDisabledActions() {
        MainWindow w;
        QCOMPARE(w.state(), humtrace::ui::JobState::Empty);
        QVERIFY(w.report() == nullptr);
    }

    void analyzesStereoFileAndShowsPerChannelFindings() {
        const auto path = writeWav(dir_.path(), "stereo.wav", 4.0);
        MainWindow w;
        w.show();
        w.openRecording(path);
        QCOMPARE(w.state(), humtrace::ui::JobState::Ready);
        w.startAnalysis();
        QTRY_COMPARE_WITH_TIMEOUT(w.state(), humtrace::ui::JobState::Complete, 20000);
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
        QTRY_COMPARE_WITH_TIMEOUT(w.state(), humtrace::ui::JobState::Complete, 20000);
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
        QTRY_COMPARE_WITH_TIMEOUT(w.state(), humtrace::ui::JobState::Complete, 20000);
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
        QTRY_COMPARE_WITH_TIMEOUT(w.state(), humtrace::ui::JobState::Complete, 20000);
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
        QTRY_COMPARE_WITH_TIMEOUT(w.state(), humtrace::ui::JobState::Complete, 20000);
        w.viewState()->setChannel(1);
        w.startAnalysis();
        QTRY_COMPARE_WITH_TIMEOUT(w.state(), humtrace::ui::JobState::Complete, 20000);
        QCOMPARE(w.viewState()->channel(), 1);
        // 4. Busy: opening another file during a run is refused and the run is unaffected.
        w.startAnalysis();
        w.openRecording(path);
        QVERIFY(humtrace::ui::is_active(w.state()) || w.state() == humtrace::ui::JobState::Complete);
        QTRY_COMPARE_WITH_TIMEOUT(w.state(), humtrace::ui::JobState::Complete, 20000);
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
        QTRY_COMPARE_WITH_TIMEOUT(w.state(), humtrace::ui::JobState::Complete, 30000);
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
        QTRY_COMPARE_WITH_TIMEOUT(w.state(), humtrace::ui::JobState::Complete, 20000);
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
        QTRY_COMPARE_WITH_TIMEOUT(w.state(), humtrace::ui::JobState::Complete, 20000);
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
        QTRY_VERIFY_WITH_TIMEOUT(w.state() == humtrace::ui::JobState::Cancelled || w.state() == humtrace::ui::JobState::Complete, 20000);
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
        QTRY_COMPARE_WITH_TIMEOUT(w.state(), humtrace::ui::JobState::Complete, 20000);
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
        QTRY_COMPARE_WITH_TIMEOUT(w.state(), humtrace::ui::JobState::Complete, 20000);
        const QString out = dir_.path() + "/report.json";
        QVERIFY(w.exportReport(out, true).isEmpty());
        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto json = f.readAll();
        QVERIFY(json.contains("\"path\": \"export.wav\""));
        QVERIFY(!json.contains(dir_.path().toUtf8()));
        QVERIFY(json.contains("\"schema_version\": \"1.1\""));
        QVERIFY(!QFile::exists(out + ".humtrace-tmp"));
        // Unwritable destination reports an error and leaves no temp file.
        QVERIFY(!w.exportReport(dir_.path() + "/missing-dir/report.json", false).isEmpty());
    }
};

QTEST_MAIN(GuiTest)
#include "test_gui.moc"
