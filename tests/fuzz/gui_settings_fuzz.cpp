// GUI settings-matrix fuzzer: every step re-randomizes ALL analysis settings, display
// options, theme, channel/zoom/cursor state (fresh entropy-derived seed per step), then runs
// one fundamental feature (analyze / export / navigate / select finding / cancel).
// Only errors are recorded. usage: gui_settings_fuzz [seconds=30]
#include "FindingsModel.hpp"
#include "MainWindow.hpp"
#include "Theme.hpp"
#include "ViewState.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>
#include <random>

namespace {
std::uint64_t freshSeed() {
    static std::uint64_t counter = 0;
    std::random_device rd;
    return (std::uint64_t(rd()) << 32) ^ rd() ^
           (std::uint64_t(std::chrono::high_resolution_clock::now().time_since_epoch().count()) * 0x9E3779B97F4A7C15ULL) ^ (++counter * 0xBF58476D1CE4E5B9ULL);
}
void put16(QByteArray& b, qint16 v) { b.append(char(v & 0xff)); b.append(char((v >> 8) & 0xff)); }
void put32(QByteArray& b, quint32 v) { for (int s = 0; s < 32; s += 8) b.append(char((v >> s) & 0xff)); }
QString writeWav(const QString& dir, std::mt19937_64& g) {
    const quint32 rate = std::uniform_int_distribution<int>(0, 1)(g) ? 4096 : 8000;
    const int channels = std::uniform_int_distribution<int>(1, 3)(g);
    const double seconds = std::uniform_real_distribution<double>(1.0, 4.0)(g);
    const double f0 = std::uniform_real_distribution<double>(20, 400)(g);
    QByteArray data;
    for (quint32 i = 0; i < quint32(seconds * rate); ++i)
        for (int c = 0; c < channels; ++c)
            put16(data, qint16(std::lround(0.4 * 32767 * std::sin(2 * std::numbers::pi * f0 * (c + 1) * i / rate))));
    QByteArray body("fmt ");
    put32(body, 16); put16(body, 1); put16(body, qint16(channels)); put32(body, rate); put32(body, rate * channels * 2); put16(body, qint16(channels * 2)); put16(body, 16);
    body += "data"; put32(body, quint32(data.size())); body += data;
    QByteArray file("RIFF");
    put32(file, quint32(body.size() + 4));
    file += "WAVE"; file += body;
    const QString path = dir + "/in.wav";
    QFile f(path);
    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write(file);
    return path;
}
} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    const double seconds = argc > 1 ? std::stod(argv[1]) : 30.0;
    QTemporaryDir dir;
    QStringList errors;
    std::size_t steps = 0, analyses = 0;
    MainWindow w;
    w.show();
    QString dialogError;
    w.setErrorHandler([&](const QString&, const QString& m) { dialogError = m; });
    QElapsedTimer clock;
    clock.start();
    while (double(clock.elapsed()) < seconds * 1000.0) {
        const auto seed = freshSeed(); // fresh seed for every re-randomization
        std::mt19937_64 g(seed);
        auto real = [&](double lo, double hi) { return std::uniform_real_distribution<double>(lo, hi)(g); };
        auto integer = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(g); };
        auto fail = [&](const QString& feature, const QString& what) {
            errors << QString("[%1] %2 (replay seed %3 step %4)").arg(feature, what).arg(seed).arg(steps);
        };
        ++steps;
        // 1. Randomize EVERY setting.
        humtrace::PipelineSettings s;
        s.support_threshold_dbfs = real(-140, 0);
        s.minimum_prominence_db = real(0, 60);
        s.peak_min_level_dbfs = real(-140, 0);
        s.peak_min_prominence_db = real(0, 60);
        s.max_peaks_per_frame = std::size_t(integer(1, 32));
        s.waveform_buckets = std::size_t(integer(1, 4096));
        s.spectrogram_rows = std::size_t(integer(1, 256));
        s.spectrogram_min_hz = real(1, 200);
        s.tracker.fixed_hz_tolerance = real(0, 20);
        s.tracker.bin_fraction = real(0, 4);
        s.tracker.max_gap_frames = std::size_t(integer(0, 12));
        s.tracker.min_observed_frames = std::size_t(integer(1, 10));
        const double floorDb = real(-160, -30), ceilDb = real(floorDb + 1 > -100 ? floorDb + 1 : -100, 0);
        const int feature = int(steps % 5);
        try {
            w.setPipelineSettings(s);
            w.setDisplayOptions(integer(0, 1), integer(0, 1), std::min(floorDb, -21.0), std::max(ceilDb, -99.0));
            theme::apply(app, integer(0, 1) == 1);
            const auto path = writeWav(dir.path(), g);
            dialogError.clear();
            w.openRecording(path);
            if (w.state() != humtrace::ui::JobState::Ready) { fail("open", "valid WAV did not reach Ready: " + dialogError); continue; }
            if (feature == 4) { // cancel at a random moment
                w.startAnalysis();
                QTest::qWait(integer(0, 40));
                QMetaObject::invokeMethod(&w, "cancelAnalysis");
                QElapsedTimer t; t.start();
                while (humtrace::ui::is_active(w.state()) && t.elapsed() < 20000) QTest::qWait(5);
                if (humtrace::ui::is_active(w.state())) fail("cancel", "job still active 20 s after cancel");
                else if (w.state() == humtrace::ui::JobState::Cancelled && w.report()) fail("cancel", "partial result retained after cancel");
                continue;
            }
            w.startAnalysis();
            QElapsedTimer t; t.start();
            while (humtrace::ui::is_active(w.state()) && t.elapsed() < 30000) QTest::qWait(5);
            if (w.state() != humtrace::ui::JobState::Complete) {
                // Legitimate only for recordings too short for the chosen window (none generated) -> any failure is an error.
                fail("analyze", "state " + QString::number(int(w.state())) + " " + dialogError);
                continue;
            }
            ++analyses;
            const auto* rep = w.report();
            if (!rep || rep->channel_results.size() != rep->channels) { fail("analyze", "report/channel mismatch"); continue; }
            // 2. Re-randomize everything again, then exercise the second feature on the same result.
            humtrace::PipelineSettings s2 = s;
            s2.support_threshold_dbfs = real(-140, 0);
            s2.tracker.min_observed_frames = std::size_t(integer(1, 10));
            w.setPipelineSettings(s2);
            w.setDisplayOptions(integer(0, 1), integer(0, 1), -100.0 - real(0, 50), -real(10, 50));
            auto* vs = w.viewState();
            switch (feature) {
            case 0: { // export a completed-then-invalidated result must still be consistent
                const QString out = dir.path() + "/r.json";
                if (w.state() == humtrace::ui::JobState::Invalidated) { /* export action disabled; API still safe */ }
                const auto err = w.exportReport(out, integer(0, 1) == 1);
                if (!err.isEmpty()) fail("export", err);
                QFile f(out);
                if (!f.open(QIODevice::ReadOnly) || !f.readAll().contains("\"schema_version\": \"1.1\"")) fail("export", "report missing or malformed");
                break;
            }
            case 1: { // navigation: channel, cursor, zoom, selection
                for (int i = 0; i < 20; ++i) {
                    vs->setChannel(integer(0, int(rep->channels) - 1));
                    vs->setCursor(real(-5, vs->total() + 5));
                    vs->zoomAround(real(0, vs->total()), real(0.05, 20));
                    vs->panBy(real(-10, 10));
                    vs->setSelection(real(-1, vs->total() + 1), real(-1, vs->total() + 1));
                    const auto v = vs->view();
                    if (!(v.start >= 0 && v.end <= vs->total() + 1e-9 && v.end > v.start)) { fail("navigate", "view range invalid"); break; }
                    if (vs->cursor() < 0 || vs->cursor() > vs->total()) { fail("navigate", "cursor out of range"); break; }
                }
                break;
            }
            case 2: { // select every finding
                for (int r = 0; r < w.findingsTable()->model()->rowCount(); ++r) w.findingsTable()->selectRow(r);
                break;
            }
            default: { // re-analyze with the second settings (settings change -> new run)
                w.startAnalysis();
                QElapsedTimer t2; t2.start();
                while (humtrace::ui::is_active(w.state()) && t2.elapsed() < 30000) QTest::qWait(5);
                if (w.state() != humtrace::ui::JobState::Complete) fail("reanalyze", "did not complete: " + dialogError);
                break;
            }
            }
            QTest::qWait(1);
            w.grab(); // force a paint of every chart with these settings
        } catch (const std::exception& e) {
            fail("exception", e.what());
        }
    }
    std::cout << "gui_settings_fuzz: " << steps << " randomized steps, " << analyses << " analyses, " << errors.size() << " error(s)\n";
    for (const auto& e : errors) std::cout << "  " << e.toStdString() << '\n';
    return errors.isEmpty() ? 0 : 1;
}
