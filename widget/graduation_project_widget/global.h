#ifndef GLOBAL_H
#define GLOBAL_H

#include <QMetaType>
#include <QString>
#include <QtGlobal>

// 连接参数，页面层通过这个结构把服务器信息交给网络管理器。
struct ServerInfo {
    QString host = "127.0.0.1";
    quint16 port = 9000;
};

// 推理结果结构，网络层解析完成后直接交给页面层展示。
struct PredictResult {
    QString pred_label;
    QString confidence;
    QString alert_level;
    QString latency_ms;
};

enum class ClientState {
    Disconnected,
    Connecting,
    Connected,
};

Q_DECLARE_METATYPE(ServerInfo)
Q_DECLARE_METATYPE(PredictResult)
Q_DECLARE_METATYPE(ClientState)

#endif // GLOBAL_H
