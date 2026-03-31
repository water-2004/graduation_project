#ifndef GLOBAL_H
#define GLOBAL_H

#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

// 连接参数，页面层通过这个结构把服务器信息交给网络管理器。
struct ServerInfo {
    QString host = "127.0.0.1";
    quint16 port = 9000;
};

// 登录成功后的用户会话信息，供主窗口切页与后续业务页面复用。
struct LoginUserInfo {
    QString username;
    QString role;
    QString display_name;
};

// 病人基础信息，供业务服务端管理页面与监测页面共用。
struct PatientInfo {
    QString patient_id;
    QString name;
    QString gender;
    int age = 0;
    QString phone;
    QString remark;
};

// 监测记录：表示一次推理结果的业务侧保存对象。
struct MonitorRecordInfo {
    QString record_id;
    QString patient_id;
    QString recorded_at;
    QString pred_label;
    QString confidence;
    QString alert_level;
    QString latency_ms;
    QString source;
    QString sample_name;
    QString true_label;
};

// 报警记录：当监测结果达到告警阈值时自动生成。
struct AlertInfo {
    QString alert_id;
    QString patient_id;
    QString created_at;
    QString alert_level;
    QString pred_label;
    QString confidence;
    QString source;
    QString sample_name;
    QString status;
    QString confirmed_at;
    QString confirmed_by;
};

// 预测结果结构，网络层解析完成后直接交给页面层展示。
struct PredictResult {
    QString pred_label;
    QString confidence;
    QString alert_level;
    QString latency_ms;
};

// 单拍心电响应：包含样本名、真实标签、预测结果和波形点。
struct BeatResponse {
    QString sample_name;
    QString true_label;
    QString pred_label;
    QString confidence;
    QString alert_level;
    QString latency_ms;
    QVector<double> values;
};

enum class ClientState {
    Disconnected,
    Connecting,
    Connected,
};

Q_DECLARE_METATYPE(ServerInfo)
Q_DECLARE_METATYPE(LoginUserInfo)
Q_DECLARE_METATYPE(PatientInfo)
Q_DECLARE_METATYPE(QVector<PatientInfo>)
Q_DECLARE_METATYPE(MonitorRecordInfo)
Q_DECLARE_METATYPE(QVector<MonitorRecordInfo>)
Q_DECLARE_METATYPE(AlertInfo)
Q_DECLARE_METATYPE(QVector<AlertInfo>)
Q_DECLARE_METATYPE(PredictResult)
Q_DECLARE_METATYPE(BeatResponse)
Q_DECLARE_METATYPE(ClientState)

#endif // GLOBAL_H
