#include "main_window.hpp"

#include <QApplication>
#include <QIcon>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("HumTrace"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/humtrace.svg")));
    QApplication::setStyle(QStringLiteral("Fusion"));
    MainWindow window;
    window.show();
    const auto args = QApplication::arguments();
    if (args.size() > 1) window.openFile(args.at(1));
    return QApplication::exec();
}
