#ifndef TCPMGR_H
#define TCPMGR_H

#include <QByteArray>
#include <QMap>
#include <QObject>
#include <QTcpSocket>

#include <functional>

#include "global.h"
#include "singleton.h"

// 网络层管理器：专门负责 TCP 连接、协议收发和响应分发。
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

    void initHandlers();
    void processBuffer();
    void handleLine(const QString& line);
    void sendLine(const QString& line);
    void emitLog(const QString& text);

    QTcpSocket _socket;
    QByteArray _buffer;
    ClientState _state;
    QMap<QString, std::function<void(const QString& line)>> _handlers;

public slots:
    void slot_tcp_connect(ServerInfo server_info);
    void slot_disconnect();
    void slot_send_ping();
    void slot_send_predict(const QString& payload);

signals:
    void sig_con_success(bool bsuccess);
    void sig_connection_closed();
    void sig_state_changed(ClientState state);
    void sig_log_message(const QString& text);
    void sig_pong();
    void sig_predict_result(const PredictResult& result);
    void sig_server_error(const QString& text);
    void sig_protocol_error(const QString& text);
};

#endif // TCPMGR_H
