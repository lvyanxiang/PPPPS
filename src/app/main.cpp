#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("PPPPS POC"));
    QApplication::setOrganizationName(QStringLiteral("PPPPS"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("PPPPS Qt desktop POC"));
    parser.addHelpOption();
    const QCommandLineOption smokeTestOption(
        QStringLiteral("smoke-test"),
        QStringLiteral("启动窗口并在事件循环运行后自动退出。")
    );
    parser.addOption(smokeTestOption);
    parser.process(application);

    MainWindow window;
    window.show();
    if (parser.isSet(smokeTestOption)) {
        QTimer::singleShot(50, &application, &QCoreApplication::quit);
    }
    return application.exec();
}
