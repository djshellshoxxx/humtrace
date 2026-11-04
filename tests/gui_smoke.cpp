// Headless GUI smoke test: run with QT_QPA_PLATFORM=offscreen.
#include "main_window.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTabWidget>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <cmath>
#include <numbers>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {
void require(bool ok, const char* msg) { if (!ok) { std::cerr << "FAIL: " << msg << '\n'; std::exit(1); } }
void le(QByteArray& b, std::uint32_t v, int n) { for (int i = 0; i < n; ++i) b.append(char((v >> (8 * i)) & 255)); }
QByteArray wav(unsigned rate, unsigned frames) {
    QByteArray d;
    for (unsigned i = 0; i < frames; ++i)
        le(d, std::uint16_t(std::int16_t(std::lround(0.5 * 32767 * std::sin(2 * std::numbers::pi * 60.0 * i / rate)))), 2);
    QByteArray w("RIFF"); le(w, 36 + std::uint32_t(d.size()), 4); w += "WAVEfmt ";
    le(w, 16, 4); le(w, 1, 2); le(w, 1, 2); le(w, rate, 4); le(w, rate * 2, 4); le(w, 2, 2); le(w, 16, 2);
    w += "data"; le(w, std::uint32_t(d.size()), 4); return w + d;
}
QByteArray toneWav(unsigned rate, unsigned frames, double a60, double a1k) {
    QByteArray d;
    for (unsigned i = 0; i < frames; ++i) {
        const double v = a60 * std::sin(2 * std::numbers::pi * 60.0 * i / rate) +
                         a1k * std::sin(2 * std::numbers::pi * 1000.0 * i / rate);
        le(d, std::uint16_t(std::int16_t(std::lround(32767 * v))), 2);
    }
    QByteArray w("RIFF"); le(w, 36 + std::uint32_t(d.size()), 4); w += "WAVEfmt ";
    le(w, 16, 4); le(w, 1, 2); le(w, 1, 2); le(w, rate, 4); le(w, rate * 2, 4); le(w, 2, 2); le(w, 16, 2);
    w += "data"; le(w, std::uint32_t(d.size()), 4); return w + d;
}
void writeFile(const QString& path, const QByteArray& data) {
    QFile f(path); require(f.open(QIODevice::WriteOnly), "write file"); f.write(data);
}
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QTemporaryDir dir;
    require(dir.isValid(), "temp dir");
    QFile f(dir.filePath("t.wav"));
    require(f.open(QIODevice::WriteOnly), "write wav");
    f.write(wav(4096, 4096 * 4)); f.close();

    MainWindow w;
    w.show();
    // 1. Successful analysis off the UI thread.
    QSignalSpy done(&w, &MainWindow::analysisFinished);
    w.openFile(f.fileName());
    require(w.analysisRunning(), "analysis starts in background");
    require(done.wait(20000) && done.first().first().toBool(), "analysis completes");
    require(w.tableRowCount() == 7, "findings table populated");
    require(w.report() && w.report()->input_sha256.size() == 64, "report has hash");
    require(!w.grab().isNull(), "window renders");
    // 2. Export, and refusal to overwrite the input.
    require(w.exportJson(dir.filePath("r.json")) && QFile::exists(dir.filePath("r.json")), "export");
    // 3. Invalid file reports an error without crashing.
    QFile bad(dir.filePath("bad.wav")); require(bad.open(QIODevice::WriteOnly), "bad"); bad.write("nope"); bad.close();
    QSignalSpy done2(&w, &MainWindow::analysisFinished);
    w.openFile(bad.fileName());
    require(done2.wait(20000) && !done2.first().first().toBool(), "invalid file fails cleanly");
    require(w.statusText().contains("failed"), "error shown");
    require(QFile(f.fileName()).size() == wav(4096, 4096 * 4).size(), "input untouched");
    // 4. Display products, tracks and compare with steady 60 Hz + 1 kHz tones.
    const QString t1 = dir.filePath("tone1.wav"), t2 = dir.filePath("tone2.wav");
    writeFile(t1, toneWav(8000, 8000 * 12, 0.3, 0.2));
    writeFile(t2, toneWav(8000, 8000 * 12, 0.2, 0.3));
    QSignalSpy done3(&w, &MainWindow::analysisFinished);
    w.openFile(t1);
    require(done3.wait(30000) && done3.first().first().toBool(), "tone analysis completes");
    require(w.trackRowCount() > 0, "tracks table populated");
    require(w.waveformView()->hasData(), "waveform has data");
    require(w.spectrogramView()->hasData() && w.spectrogramView()->columnCount() > 0, "spectrogram has data");
    w.selectTrackRow(0);
    const QString outDir = qEnvironmentVariable("HUMTRACE_GUI_SHOTS");
    for (int i = 0; i < w.viewTabs()->count(); ++i) {
        w.viewTabs()->setCurrentIndex(i);
        require(!w.viewTabs()->currentWidget()->grab().isNull(), "view grabs");
        if (!outDir.isEmpty()) w.grab().save(QDir(outDir).filePath(QStringLiteral("gui_tab%1.png").arg(i)));
    }
    w.viewTabs()->setCurrentIndex(1);
    for (auto* tw : w.findChildren<QTabWidget*>()) if (tw != w.viewTabs()) tw->setCurrentIndex(1);
    if (!outDir.isEmpty()) w.grab().save(QDir(outDir).filePath("gui_tracks.png"));
    QSignalSpy done4(&w, &MainWindow::compareFinished);
    w.compareFiles({t1, t2});
    require(w.analysisRunning(), "compare runs in background");
    require(done4.wait(60000) && done4.first().first().toBool(), "compare completes");
    require(w.comparisonText().contains("Shared"), "comparison lists shared tones");
    require(!w.exportComparisonJson(t1), "comparison export refuses an input path");
    require(w.exportComparisonJson(dir.filePath("c.json")) && QFile::exists(dir.filePath("c.json")), "comparison export");
    std::cout << "GUI smoke test passed.\n";
    return 0;
}
