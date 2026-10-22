#include "../../app/qt/MainWindow.hpp"
#include "../../app/qt/SpectrogramChart.hpp"
#include "../../app/qt/ViewState.hpp"
#include "humtrace/sha256.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QTemporaryDir>
#include <QFile>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir temporary;
    QString path = qEnvironmentVariable("HUMTRACE_MEMORY_WAV");
    if (path.isEmpty()) {
        path = temporary.filePath("small.wav");
        QByteArray pcm;
        for (unsigned i = 0; i < 40960; ++i) {
            const unsigned value = (i / 32U) % 2U ? 8000U : 57536U;
            pcm.append(char(value & 255U)); pcm.append(char(value >> 8U));
        }
        QByteArray bytes("RIFF");
        const auto put32 = [&](unsigned value) { for (unsigned n = 0; n < 4; ++n) bytes.append(char((value >> (n*8U)) & 255U)); };
        put32(static_cast<unsigned>(pcm.size()) + 36U);
        bytes.append("WAVEfmt "); put32(16);
        bytes.append(QByteArray::fromHex("01000100")); put32(4096); put32(8192);
        bytes.append(QByteArray::fromHex("02001000")); bytes.append("data");
        put32(static_cast<unsigned>(pcm.size())); bytes.append(pcm);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) return 1;
        file.close();
    }
    try {
        const auto digest = [&] { std::ifstream input(path.toStdString(), std::ios::binary); return humtrace::sha256_hex(input); };
        const auto before = digest();
        MainWindow window;
        window.setPagedAnalysis(true);
        QString failure;
        window.setErrorHandler([&](const QString& stage, const QString& message) { failure = stage + ": " + message; });
        window.show();
        std::uint64_t heartbeats = 0;
        QTimer heartbeat;
        QObject::connect(&heartbeat, &QTimer::timeout, [&] { ++heartbeats; });
        heartbeat.start(10);
        window.openRecording(path);
        window.startAnalysis();
        QElapsedTimer deadline;
        deadline.start();
        while (window.state() != humtrace::ui::JobState::Complete && failure.isEmpty() && deadline.elapsed() < 180000) {
            app.processEvents();
            QThread::msleep(1);
        }
        if (!failure.isEmpty()) throw std::runtime_error(failure.toStdString());
        if (window.state() != humtrace::ui::JobState::Complete || !window.hasPagedResults() || !heartbeats)
            throw std::runtime_error("paged job did not complete with a live GUI heartbeat");
        auto* chart = window.findChild<SpectrogramChart*>();
        if (!chart) throw std::runtime_error("spectrogram missing");
        for (int i = 0; i < 64; ++i) {
            const double time = window.viewState()->total() * static_cast<double>(i) / 64.0;
            window.viewState()->setView({time, time + 10.0});
            window.viewState()->setCursor(time);
            app.processEvents();
            if (window.pagedCacheBytes() > 6U*1024U*1024U || chart->imageBytes() > 2U*1024U*1024U)
                throw std::runtime_error("GUI cache budget exceeded");
        }
        if (digest() != before) throw std::runtime_error("source was modified");
        std::cout << "GUI_PAGED cache=" << window.pagedCacheBytes() << " image=" << chart->imageBytes()
                  << " heartbeats=" << heartbeats << " elapsed_ms=" << deadline.elapsed() << '\n';
        window.close();
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
