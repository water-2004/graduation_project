#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFile>

#include <filesystem>

#include "global.h"
#include "logger.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFile qss_file(":/styles/global.qss");
    if (qss_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        a.setStyleSheet(QString::fromUtf8(qss_file.readAll()));
    }
    qRegisterMetaType<ServerInfo>("ServerInfo");
    qRegisterMetaType<LoginUserInfo>("LoginUserInfo");
    qRegisterMetaType<PatientInfo>("PatientInfo");
    qRegisterMetaType<QVector<PatientInfo>>("QVector<PatientInfo>");
    qRegisterMetaType<MonitorRecordInfo>("MonitorRecordInfo");
    qRegisterMetaType<QVector<MonitorRecordInfo>>("QVector<MonitorRecordInfo>");
    qRegisterMetaType<AlertInfo>("AlertInfo");
    qRegisterMetaType<QVector<AlertInfo>>("QVector<AlertInfo>");
    qRegisterMetaType<PredictResult>("PredictResult");
    qRegisterMetaType<BeatResponse>("BeatResponse");
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
