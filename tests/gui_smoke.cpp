// Headless GUI smoke test: run with QT_QPA_PLATFORM=offscreen.
#include "main_window.hpp"

#include <QApplication>
#include <QFile>
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
    std::cout << "GUI smoke test passed.\n";
    return 0;
}
