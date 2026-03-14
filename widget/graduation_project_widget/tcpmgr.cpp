#include "tcpmgr.h"

#include <QAbstractSocket>
#include <QRegularExpression>

#include "logger.h"

TcpMgr::TcpMgr()
    : QObject(nullptr)
    , _state(ClientState::Disconnected)
{
    QObject::connect(&_socket, &QTcpSocket::connected, this, [this]() {
        _state = ClientState::Connected;
        emit sig_state_changed(_state);
        emitLog(QString("服务器: 已连接到 %1:%2")
                    .arg(_socket.peerAddress().toString())
                    .arg(_socket.peerPort()));
        emit sig_con_success(true);
    });

    QObject::connect(&_socket, &QTcpSocket::readyRead, this, [this]() {
        _buffer.append(_socket.readAll());
        processBuffer();
    });

    QObject::connect(&_socket,
                     QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
                     this,
                     [this](QAbstractSocket::SocketError) {
        const QString error_text = _socket.errorString();
        gp::logging::Logger::Instance().ErrorText(QString("网络错误: %1").arg(error_text).toUtf8().toStdString());
        emit sig_server_error(error_text);

        if (_state == ClientState::Connecting) {
            _state = ClientState::Disconnected;
            emit sig_state_changed(_state);
            emit sig_con_success(false);
        }
    });

    QObject::connect(&_socket, &QTcpSocket::disconnected, this, [this]() {
        _buffer.clear();
        _state = ClientState::Disconnected;
        emit sig_state_changed(_state);
        emitLog("服务器: 连接已断开");
        emit sig_connection_closed();
    });

    initHandlers();
}

TcpMgr::~TcpMgr() = default;

void TcpMgr::CloseConnection() {
    slot_disconnect();
}

ClientState TcpMgr::state() const {
    return _state;
}

void TcpMgr::initHandlers() {
    _handlers.insert("PONG", [this](const QString&) {
        emit sig_pong();
    });

    _handlers.insert("BYE", [this](const QString&) {
        emitLog("服务器: 已确认断开请求");
    });

    _handlers.insert("ERR", [this](const QString& line) {
        const QString error_text = line.mid(3).trimmed();
        gp::logging::Logger::Instance().WarningText(QString("服务端返回错误: %1").arg(error_text).toUtf8().toStdString());
        emit sig_server_error(error_text);
    });

    _handlers.insert("OK", [this](const QString& line) {
        static const QRegularExpression response_re(
            R"(^OK\s+pred=(\S+)\s+conf=([-\d\.eE\+]+)\s+alert=(\S+)\s+latency_ms=([-\d\.eE\+]+)$)");

        const QRegularExpressionMatch match = response_re.match(line);
        if (!match.hasMatch()) {
            const QString error_text = QString("无法解析服务端响应: %1").arg(line);
            gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
            emit sig_protocol_error(error_text);
            return;
        }

        PredictResult result;
        result.pred_label = match.captured(1);
        result.confidence = match.captured(2);
        result.alert_level = match.captured(3);
        result.latency_ms = match.captured(4);
        emit sig_predict_result(result);
    });
}

void TcpMgr::processBuffer() {
    while (true) {
        const int newline_index = _buffer.indexOf('\n');
        if (newline_index < 0) {
            return;
        }

        QByteArray line = _buffer.left(newline_index);
        _buffer.remove(0, newline_index + 1);
        if (!line.isEmpty() && line.endsWith('\r')) {
            line.chop(1);
        }

        handleLine(QString::fromUtf8(line));
    }
}

void TcpMgr::handleLine(const QString& line) {
    emitLog(QString("服务器 << %1").arg(line));

    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        const QString error_text = "收到空响应";
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_protocol_error(error_text);
        return;
    }

    const QString key = trimmed.section(' ', 0, 0).toUpper();
    const auto iter = _handlers.find(key);
    if (iter == _handlers.end()) {
        const QString error_text = QString("未注册的响应类型: %1").arg(trimmed);
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_protocol_error(error_text);
        return;
    }

    iter.value()(trimmed);
}

void TcpMgr::sendLine(const QString& line) {
    if (_state != ClientState::Connected) {
        const QString error_text = "当前未连接服务器";
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    _socket.write(line.toUtf8());
    _socket.write("\n");
}

void TcpMgr::emitLog(const QString& text) {
    gp::logging::Logger::Instance().InfoText(text.toUtf8().toStdString());
    emit sig_log_message(text);
}

void TcpMgr::slot_tcp_connect(ServerInfo server_info) {
    if (_state != ClientState::Disconnected) {
        emitLog("本地: 当前连接尚未断开，忽略新的连接请求");
        return;
    }

    if (server_info.host.trimmed().isEmpty()) {
        const QString error_text = "服务器 IP 不能为空";
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    _buffer.clear();
    _state = ClientState::Connecting;
    emit sig_state_changed(_state);
    emitLog(QString("本地: 开始连接 %1:%2").arg(server_info.host).arg(server_info.port));
    _socket.connectToHost(server_info.host, server_info.port);
}

void TcpMgr::slot_disconnect() {
    if (_state == ClientState::Disconnected) {
        return;
    }

    if (_state == ClientState::Connected) {
        _socket.write("QUIT\n");
        _socket.flush();
        emitLog("客户端 >> QUIT");
    }

    _socket.disconnectFromHost();
    if (_socket.state() != QAbstractSocket::UnconnectedState) {
        _socket.waitForDisconnected(200);
    }
}

void TcpMgr::slot_send_ping() {
    if (_state != ClientState::Connected) {
        const QString error_text = "请先连接服务器";
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    sendLine("PING");
    emitLog("客户端 >> PING");
}

void TcpMgr::slot_send_predict(const QString& payload) {
    if (_state != ClientState::Connected) {
        const QString error_text = "请先连接服务器";
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    sendLine(QString("PREDICT %1").arg(payload));
    emitLog("客户端 >> PREDICT [187 维特征]");
}
