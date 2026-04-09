#ifndef TCPMGR_H
#define TCPMGR_H

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

#include "global.h"
#include "protocol/message_protocol.h"
#include "singleton.h"

// 推理网络管理器：负责边缘推理服务的 TCP 连接、JSON 协议收发和结果分发。
class TcpMgr : public QObject, public Singleton<TcpMgr>
{
    Q_OBJECT

public:
    ~TcpMgr();

    void CloseConnection();
    ClientState state() const;

private:
    friend class Singleton<TcpMgr>;
    TcpMgr();

    void processBuffer();
    void handlePacket(const gp::protocol::Packet& packet);
    void sendPacket(gp::protocol::MessageType type, const QJsonObject& payload = QJsonObject());
    void emitLog(const QString& text);

    static constexpr int kConnectTimeoutMs = 10000;
    static constexpr int kMaxBufferSize = 10 * 1024 * 1024;
    static constexpr int kMaxReconnectDelayMs = 30000;

    QTcpSocket _socket;
    QByteArray _buffer;
    ClientState _state;

    QTimer connect_timer_;
    QTimer reconnect_timer_;
    ServerInfo last_server_info_;
    int reconnect_attempts_ = 0;
    bool user_initiated_disconnect_ = false;

public slots:
    void slot_tcp_connect(ServerInfo server_info);
    void slot_disconnect();
    void slot_send_ping();
    void slot_send_predict(const QString& payload);
    void slot_list_samples();
    void slot_play_sample(const QString& sample_name);

signals:
    void sig_con_success(bool bsuccess);
    void sig_connection_closed();
    void sig_state_changed(ClientState state);
    void sig_log_message(const QString& text);
    void sig_pong();
    void sig_predict_result(const PredictResult& result);
    void sig_sample_list_ready(const QStringList& samples);
    void sig_beat_response(const BeatResponse& response);
    void sig_server_error(const QString& text);
    void sig_protocol_error(const QString& text);
};

#endif  // TCPMGR_H
