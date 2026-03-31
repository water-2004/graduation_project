#include "tcpmgr.h"

#include <QAbstractSocket>
#include <QNetworkProxy>
#include <QLocale>
#include <QRegularExpression>

#include <algorithm>

#include "logger.h"

namespace {

bool ParseCsvValues(const QString& csv, QVector<double>* values, QString* error_text) {
    const QStringList tokens = csv.split(',', Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        if (error_text != nullptr) {
            *error_text = QStringLiteral("服务端未返回波形点");
        }
        return false;
    }

    QVector<double> parsed;
    parsed.reserve(tokens.size());
    const QLocale locale = QLocale::c();

    for (int i = 0; i < tokens.size(); ++i) {
        bool ok = false;
        const double value = locale.toDouble(tokens.at(i), &ok);
        if (!ok) {
            if (error_text != nullptr) {
                *error_text = QStringLiteral("第 %1 个波形点不是合法数字").arg(i + 1);
            }
            return false;
        }
        parsed.push_back(value);
    }

    *values = std::move(parsed);
    return true;
}

}  // namespace

TcpMgr::TcpMgr()
    : QObject(nullptr)
    , _state(ClientState::Disconnected)
{
    _socket.setProxy(QNetworkProxy::NoProxy);

    connect_timer_.setSingleShot(true);
    reconnect_timer_.setSingleShot(true);

    QObject::connect(&connect_timer_, &QTimer::timeout, this, [this]() {
        if (_state == ClientState::Connecting) {
            emitLog(QStringLiteral("本地: 连接超时，中止连接"));
            _socket.abort();
            _state = ClientState::Disconnected;
            emit sig_state_changed(_state);
            emit sig_con_success(false);
        }
    });

    QObject::connect(&reconnect_timer_, &QTimer::timeout, this, [this]() {
        if (_state == ClientState::Disconnected && !user_initiated_disconnect_) {
            emitLog(QString("本地: 尝试自动重连 (第 %1 次)").arg(reconnect_attempts_ + 1));
            slot_tcp_connect(last_server_info_);
        }
    });

    QObject::connect(&_socket, &QTcpSocket::connected, this, [this]() {
        connect_timer_.stop();
        reconnect_attempts_ = 0;
        _state = ClientState::Connected;
        emit sig_state_changed(_state);
        emitLog(QString("服务器: 已连接到 %1:%2")
                    .arg(_socket.peerAddress().toString())
                    .arg(_socket.peerPort()));
        emit sig_con_success(true);
    });

    QObject::connect(&_socket, &QTcpSocket::readyRead, this, [this]() {
        _buffer.append(_socket.readAll());
        if (_buffer.size() > kMaxBufferSize) {
            emitLog(QStringLiteral("本地: 接收缓冲区溢出，断开连接"));
            _buffer.clear();
            _socket.abort();
            return;
        }
        processBuffer();
    });

    QObject::connect(&_socket,
                     QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
                     this,
                     [this](QAbstractSocket::SocketError) {
        connect_timer_.stop();
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
        connect_timer_.stop();
        _buffer.clear();
        _state = ClientState::Disconnected;
        emit sig_state_changed(_state);
        emitLog(QStringLiteral("服务器: 连接已断开"));
        emit sig_connection_closed();

        if (!user_initiated_disconnect_ && !last_server_info_.host.isEmpty()) {
            const int delay = std::min(1000 * (1 << reconnect_attempts_), kMaxReconnectDelayMs);
            ++reconnect_attempts_;
            emitLog(QString("本地: %1 秒后尝试自动重连...").arg(delay / 1000));
            reconnect_timer_.start(delay);
        }
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
    _handlers.insert(QStringLiteral("PONG"), [this](const QString&) {
        emit sig_pong();
    });

    _handlers.insert(QStringLiteral("BYE"), [this](const QString&) {
        emitLog(QStringLiteral("服务器: 已确认断开请求"));
    });

    _handlers.insert(QStringLiteral("ERR"), [this](const QString& line) {
        const QString error_text = line.mid(3).trimmed();
        gp::logging::Logger::Instance().WarningText(QString("服务端返回错误: %1").arg(error_text).toUtf8().toStdString());
        emit sig_server_error(error_text);
    });

    _handlers.insert(QStringLiteral("OK"), [this](const QString& line) {
        static const QRegularExpression response_re(
            R"(^OK\s+pred=(\S+)\s+conf=([-\d\.eE\+]+)\s+alert=(\S+)\s+latency_ms=([-\d\.eE\+]+)$)");

        const QRegularExpressionMatch match = response_re.match(line);
        if (!match.hasMatch()) {
            const QString error_text = QStringLiteral("无法解析服务端响应: %1").arg(line);
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

    _handlers.insert(QStringLiteral("SAMPLES"), [this](const QString& line) {
        const QString payload = line.mid(QStringLiteral("SAMPLES").size()).trimmed();
        QStringList samples;
        if (!payload.isEmpty()) {
            samples = payload.split(',', Qt::SkipEmptyParts);
            for (QString& sample : samples) {
                sample = sample.trimmed();
            }
            samples.removeAll(QString());
        }
        emit sig_sample_list_ready(samples);
    });

    _handlers.insert(QStringLiteral("BEAT"), [this](const QString& line) {
        static const QRegularExpression response_re(
            R"(^BEAT\s+name=(\S+)\s+true=(\S+)\s+pred=(\S+)\s+conf=([-\d\.eE\+]+)\s+alert=(\S+)\s+latency_ms=([-\d\.eE\+]+)\s+values=(.+)$)");

        const QRegularExpressionMatch match = response_re.match(line);
        if (!match.hasMatch()) {
            const QString error_text = QStringLiteral("无法解析单拍响应: %1").arg(line.left(120));
            gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
            emit sig_protocol_error(error_text);
            return;
        }

        BeatResponse response;
        response.sample_name = match.captured(1);
        response.true_label = match.captured(2);
        response.pred_label = match.captured(3);
        response.confidence = match.captured(4);
        response.alert_level = match.captured(5);
        response.latency_ms = match.captured(6);

        QString error_text;
        if (!ParseCsvValues(match.captured(7), &response.values, &error_text)) {
            gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
            emit sig_protocol_error(error_text);
            return;
        }

        emit sig_beat_response(response);
    });
}

void TcpMgr::processBuffer() {
    int consumed = 0;
    while (true) {
        const int newline_index = _buffer.indexOf('\n', consumed);
        if (newline_index < 0) {
            break;
        }

        QByteArray line = _buffer.mid(consumed, newline_index - consumed);
        consumed = newline_index + 1;
        if (!line.isEmpty() && line.endsWith('\r')) {
            line.chop(1);
        }

        handleLine(QString::fromUtf8(line));
    }
    if (consumed > 0) {
        _buffer.remove(0, consumed);
    }
}

void TcpMgr::handleLine(const QString& line) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        const QString error_text = QStringLiteral("收到空响应");
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_protocol_error(error_text);
        return;
    }

    const QString key = trimmed.section(' ', 0, 0).toUpper();
    if (key == QStringLiteral("BEAT")) {
        emitLog(QStringLiteral("服务器 << BEAT [单拍波形与推理结果]"));
    } else if (key == QStringLiteral("SAMPLES")) {
        emitLog(QStringLiteral("服务器 << SAMPLES [样本列表]"));
    } else {
        emitLog(QString("服务器 << %1").arg(trimmed));
    }

    const auto iter = _handlers.find(key);
    if (iter == _handlers.end()) {
        const QString error_text = QStringLiteral("未注册的响应类型: %1").arg(trimmed);
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_protocol_error(error_text);
        return;
    }

    iter.value()(trimmed);
}

void TcpMgr::sendLine(const QString& line) {
    if (_state != ClientState::Connected) {
        const QString error_text = QStringLiteral("当前未连接服务器");
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
        emitLog(QStringLiteral("本地: 当前连接尚未断开，忽略新的连接请求"));
        return;
    }

    if (server_info.host.trimmed().isEmpty()) {
        const QString error_text = QStringLiteral("服务器 IP 不能为空");
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    user_initiated_disconnect_ = false;
    last_server_info_ = server_info;
    _buffer.clear();
    _state = ClientState::Connecting;
    emit sig_state_changed(_state);
    emitLog(QString("本地: 开始连接 %1:%2").arg(server_info.host).arg(server_info.port));
    _socket.connectToHost(server_info.host, server_info.port);
    connect_timer_.start(kConnectTimeoutMs);
}

void TcpMgr::slot_disconnect() {
    if (_state == ClientState::Disconnected) {
        return;
    }

    user_initiated_disconnect_ = true;
    reconnect_timer_.stop();
    connect_timer_.stop();

    if (_state == ClientState::Connected) {
        _socket.write("QUIT\n");
        _socket.flush();
        emitLog(QStringLiteral("客户端 >> QUIT"));
    }

    _socket.disconnectFromHost();
    if (_socket.state() != QAbstractSocket::UnconnectedState) {
        _socket.abort();
    }
}

void TcpMgr::slot_send_ping() {
    if (_state != ClientState::Connected) {
        const QString error_text = QStringLiteral("请先连接服务器");
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    sendLine(QStringLiteral("PING"));
    emitLog(QStringLiteral("客户端 >> PING"));
}

void TcpMgr::slot_send_predict(const QString& payload) {
    if (_state != ClientState::Connected) {
        const QString error_text = QStringLiteral("请先连接服务器");
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    sendLine(QString("PREDICT %1").arg(payload));
    emitLog(QStringLiteral("客户端 >> PREDICT [187 维特征]"));
}

void TcpMgr::slot_list_samples() {
    if (_state != ClientState::Connected) {
        const QString error_text = QStringLiteral("请先连接服务器");
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    sendLine(QStringLiteral("LIST_SAMPLES"));
    emitLog(QStringLiteral("客户端 >> LIST_SAMPLES"));
}

void TcpMgr::slot_play_sample(const QString& sample_name) {
    if (_state != ClientState::Connected) {
        const QString error_text = QStringLiteral("请先连接服务器");
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    if (sample_name.trimmed().isEmpty()) {
        const QString error_text = QStringLiteral("样本名不能为空");
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    sendLine(QString("PLAY_SAMPLE %1").arg(sample_name.trimmed()));
    emitLog(QString("客户端 >> PLAY_SAMPLE %1").arg(sample_name.trimmed()));
}

