#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>

#include <filesystem>

#include "global.h"
#include "logger.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qRegisterMetaType<ServerInfo>("ServerInfo");
    qRegisterMetaType<PredictResult>("PredictResult");
    qRegisterMetaType<ClientState>("ClientState");

    gp::logging::LoggerOptions log_options;
    log_options.app_name = "graduation_project_widget";
    log_options.log_dir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdString()) / "logs";
    log_options.min_level = gp::logging::LogLevel::Debug;
    gp::logging::Logger::Instance().Initialize(log_options);
    gp::logging::Logger::Instance().Info("Qt 客户端启动");

    QObject::connect(&a, &QCoreApplication::aboutToQuit, []() {
        gp::logging::Logger::Instance().Info("Qt 客户端退出");
    });

    MainWindow w;
    w.show();
    return a.exec();
}
