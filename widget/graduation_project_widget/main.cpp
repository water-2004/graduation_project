#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <filesystem>

#include "ViewModels/AppViewModel.h"
#include "global.h"
#include "logger.h"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("graduation_project_widget"));
    app.setOrganizationName(QStringLiteral("graduation_project"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 10));

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
    gp::logging::Logger::Instance().Info("QML 客户端启动");

    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        gp::logging::Logger::Instance().Info("QML 客户端退出");
    });

    AppViewModel app_view_model;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appViewModel"), &app_view_model);

    const QUrl url(QStringLiteral("qrc:/qt/qml/graduation_project/widget/UI/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
