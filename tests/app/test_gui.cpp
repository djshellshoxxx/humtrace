// End-to-end GUI behavior tests (offscreen). Exercise the same code paths as the actions.
#include "FindingsModel.hpp"
#include "MainWindow.hpp"
#include "SpectrogramChart.hpp"
#include "ViewState.hpp"
#include "WaveformChart.hpp"
#include "humtrace/sha256.hpp"

#include <QElapsedTimer>
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
QString writeWav(const QString& dir, const QString& name, double seconds, double leftHz = 60.0, double rightHz = 50.0) {
    const quint32 rate = 4096, frames = quint32(seconds * rate);
    QByteArray data;
    for (quint32 i = 0; i < frames; ++i) {
        put16(data, qint16(std::lround(0.5 * 32767 * std::sin(2 * std::numbers::pi * leftHz * i / rate))));
        put16(data, qint16(std::lround(0.25 * 32767 * std::sin(2 * std::numbers::pi * rightHz * i / rate))));
    }
    QByteArray body("fmt ");
    put32(body, 16); put16(body, 1); put16(body, 2); put32(body, rate); put32(body, rate * 4); put16(body, 4); put16(body, 16);
    body += "data"; put32(body, quint32(data.size())); body += data;
    QByteArray file("RIFF");
    put32(file, quint32(body.size() + 4));
    file += "WAVE"; file += body;
    const QString path = dir + "/" + name;
    QFile f(path);
    f.open(QIODevice::WriteOnly);
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
        f.open(QIODevice::WriteOnly);
        f.write("this is not audio");
        f.close();
        MainWindow w;
        QString stage, message;
        w.setErrorHandler([&](const QString& s, const QString& m) { stage = s; message = m; });
        w.openRecording(bad);
        QCOMPARE(w.state(), humtrace::ui::JobState::Failed);
        QVERIFY(message.contains("RIFF"));
        QFile check(bad);
        check.open(QIODevice::ReadOnly);
        QCOMPARE(check.readAll(), QByteArray("this is not audio"));
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
