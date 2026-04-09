#include "tcpmgr.h"

#include <QAbstractSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLocale>
#include <QNetworkProxy>

#include <algorithm>

#include "logger.h"
#include "protocol_codec.h"

namespace {

QString FormatNumber(double value, int precision = 6) {
    QString text = QString::number(value, 'f', precision);
    while (text.contains('.') && (text.endsWith('0') || text.endsWith('.'))) {
        if (text.endsWith('.')) {
            text.chop(1);
            break;
        }
        text.chop(1);
    }
    if (text.isEmpty()) {
        text = QStringLiteral("0");
    }
    return text;
}

QString JsonValueToText(const QJsonValue& value) {
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return FormatNumber(value.toDouble());
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isNull() || value.isUndefined()) {
        return QString();
    }
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    return QString();
}

bool ParseCsvValues(const QString& csv, QVector<double>* values, QString* error_text) {
    if (values == nullptr) {
        if (error_text != nullptr) {
            *error_text = QStringLiteral("内部错误：输出波形数组参数为空");
        }
        return false;
    }

    const QStringList tokens = csv.split(',', Qt::SkipEmptyParts);
    if (tokens.isEmpty()) {
        if (error_text != nullptr) {
            *error_text = QStringLiteral("没有可发送的特征值");
        }
        return false;
    }

    QVector<double> parsed;
    parsed.reserve(tokens.size());
    const QLocale locale = QLocale::c();

    for (int i = 0; i < tokens.size(); ++i) {
        bool ok = false;
        const double value = locale.toDouble(tokens.at(i).trimmed(), &ok);
        if (!ok) {
            if (error_text != nullptr) {
                *error_text = QStringLiteral("第 %1 个特征值不是合法数字").arg(i + 1);
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
    , _state(ClientState::Disconnected) {
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
}

TcpMgr::~TcpMgr() = default;

void TcpMgr::CloseConnection() {
    slot_disconnect();
}

ClientState TcpMgr::state() const {
    return _state;
}

void TcpMgr::processBuffer() {
    while (true) {
        gp::protocol::Packet packet;
        QString error_text;
        const auto status = gp::qt_protocol::TryTakePacket(&_buffer, &packet, &error_text);
        if (status == gp::qt_protocol::PacketDecodeStatus::kNeedMoreData) {
            break;
        }
        if (status == gp::qt_protocol::PacketDecodeStatus::kError) {
            gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
            emit sig_protocol_error(error_text);
            _buffer.clear();
            _socket.abort();
            return;
        }
        handlePacket(packet);
    }
}

void TcpMgr::handlePacket(const gp::protocol::Packet& packet) {
    QJsonObject payload;
    QString error_text;
    if (!gp::qt_protocol::ParseJsonObject(packet, &payload, &error_text)) {
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_protocol_error(error_text);
        return;
    }

    emitLog(QString("服务器 << %1").arg(gp::qt_protocol::MessageTypeToText(packet.message_type)));

    switch (packet.message_type) {
    case gp::protocol::MessageType::kPingResponse:
        emit sig_pong();
        return;

    case gp::protocol::MessageType::kByeResponse:
        emitLog(QStringLiteral("服务器: 已确认断开请求"));
        return;

    case gp::protocol::MessageType::kErrorResponse: {
        const QString message = JsonValueToText(payload.value(QStringLiteral("message"))).trimmed();
        const QString code = JsonValueToText(payload.value(QStringLiteral("code"))).trimmed();
        const QString final_text = code.isEmpty() ? message : QStringLiteral("%1: %2").arg(code, message);
        gp::logging::Logger::Instance().WarningText(final_text.toUtf8().toStdString());
        emit sig_server_error(final_text);
        return;
    }

    case gp::protocol::MessageType::kPredictResponse: {
        PredictResult result;
        result.pred_label = JsonValueToText(payload.value(QStringLiteral("pred_label")));
        result.confidence = JsonValueToText(payload.value(QStringLiteral("confidence")));
        result.alert_level = JsonValueToText(payload.value(QStringLiteral("alert_level")));
        result.latency_ms = JsonValueToText(payload.value(QStringLiteral("latency_ms")));
        emit sig_predict_result(result);
        return;
    }

    case gp::protocol::MessageType::kListSamplesResponse: {
        QStringList samples;
        const QJsonArray sample_array = payload.value(QStringLiteral("samples")).toArray();
        for (const QJsonValue& value : sample_array) {
            const QString sample_name = JsonValueToText(value).trimmed();
            if (!sample_name.isEmpty()) {
                samples.push_back(sample_name);
            }
        }
        emit sig_sample_list_ready(samples);
        return;
    }

    case gp::protocol::MessageType::kPlaySampleResponse: {
        BeatResponse response;
        response.sample_name = JsonValueToText(payload.value(QStringLiteral("sample_name")));
        response.true_label = JsonValueToText(payload.value(QStringLiteral("true_label")));
        response.pred_label = JsonValueToText(payload.value(QStringLiteral("pred_label")));
        response.confidence = JsonValueToText(payload.value(QStringLiteral("confidence")));
        response.alert_level = JsonValueToText(payload.value(QStringLiteral("alert_level")));
        response.latency_ms = JsonValueToText(payload.value(QStringLiteral("latency_ms")));

        const QJsonArray values = payload.value(QStringLiteral("values")).toArray();
        response.values.reserve(values.size());
        for (const QJsonValue& value : values) {
            if (value.isDouble()) {
                response.values.push_back(value.toDouble());
                continue;
            }

            bool ok = false;
            const double numeric_value = QLocale::c().toDouble(JsonValueToText(value), &ok);
            if (!ok) {
                const QString text = QStringLiteral("播放样本响应中的波形点不是合法数字");
                gp::logging::Logger::Instance().WarningText(text.toUtf8().toStdString());
                emit sig_protocol_error(text);
                return;
            }
            response.values.push_back(numeric_value);
        }

        emit sig_beat_response(response);
        return;
    }

    default: {
        const QString text = QStringLiteral("未注册的推理响应类型: %1")
                                 .arg(gp::qt_protocol::MessageTypeToText(packet.message_type));
        gp::logging::Logger::Instance().WarningText(text.toUtf8().toStdString());
        emit sig_protocol_error(text);
        return;
    }
    }
}

void TcpMgr::sendPacket(gp::protocol::MessageType type, const QJsonObject& payload) {
    if (_state != ClientState::Connected) {
        const QString error_text = QStringLiteral("当前未连接服务器");
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    const QByteArray bytes = gp::qt_protocol::EncodeJsonPacket(type, payload);
    _socket.write(bytes);
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
        const QByteArray bytes = gp::qt_protocol::EncodeJsonPacket(gp::protocol::MessageType::kQuitRequest);
        _socket.write(bytes);
        _socket.flush();
        emitLog(QStringLiteral("客户端 >> QuitRequest"));
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

    sendPacket(gp::protocol::MessageType::kPingRequest);
    emitLog(QStringLiteral("客户端 >> PingRequest"));
}

void TcpMgr::slot_send_predict(const QString& payload) {
    if (_state != ClientState::Connected) {
        const QString error_text = QStringLiteral("请先连接服务器");
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    QVector<double> values;
    QString error_text;
    if (!ParseCsvValues(payload, &values, &error_text)) {
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    QJsonArray features;
    for (double value : values) {
        features.append(value);
    }

    QJsonObject request;
    request.insert(QStringLiteral("features"), features);
    sendPacket(gp::protocol::MessageType::kPredictRequest, request);
    emitLog(QStringLiteral("客户端 >> PredictRequest [187 维特征]"));
}

void TcpMgr::slot_list_samples() {
    if (_state != ClientState::Connected) {
        const QString error_text = QStringLiteral("请先连接服务器");
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    sendPacket(gp::protocol::MessageType::kListSamplesRequest);
    emitLog(QStringLiteral("客户端 >> ListSamplesRequest"));
}

void TcpMgr::slot_play_sample(const QString& sample_name) {
    if (_state != ClientState::Connected) {
        const QString error_text = QStringLiteral("请先连接服务器");
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    const QString trimmed_name = sample_name.trimmed();
    if (trimmed_name.isEmpty()) {
        const QString error_text = QStringLiteral("样本名不能为空");
        gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
        emit sig_server_error(error_text);
        return;
    }

    QJsonObject request;
    request.insert(QStringLiteral("sample_name"), trimmed_name);
    sendPacket(gp::protocol::MessageType::kPlaySampleRequest, request);
    emitLog(QString("客户端 >> PlaySampleRequest %1").arg(trimmed_name));
}

