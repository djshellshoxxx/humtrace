#include "MainWindow.hpp"
#include "Theme.hpp"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("HumTrace");
    QApplication::setOrganizationName("Circuit Drift Labs");
    QApplication::setApplicationVersion(HUMTRACE_VERSION);
    theme::apply(app, false);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "WAV recording to open");
    parser.process(app);
    MainWindow window;
    window.show();
    if (!parser.positionalArguments().isEmpty()) window.openRecording(parser.positionalArguments().first());
    return app.exec();
}
