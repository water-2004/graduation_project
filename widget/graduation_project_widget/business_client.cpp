#include "business_client.h"

#include <QAbstractSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkProxy>

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

QString JsonValueToTextLocal(const QJsonValue& value) {
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

QString BuildErrorText(const QJsonObject& payload) {
    const QString code = JsonValueToTextLocal(payload.value(QStringLiteral("code"))).trimmed();
    const QString message = JsonValueToTextLocal(payload.value(QStringLiteral("message"))).trimmed();
    if (code.isEmpty()) {
        return message;
    }
    if (message.isEmpty()) {
        return code;
    }
    return QStringLiteral("%1: %2").arg(code, message);
}

}  // namespace

BusinessClient::BusinessClient(QObject* parent)
    : QObject(parent) {
    socket_.setProxy(QNetworkProxy::NoProxy);
    connect_timer_.setSingleShot(true);

    QObject::connect(&connect_timer_, &QTimer::timeout, this, [this]() {
        if (state_ == ClientState::Connecting) {
            EmitLog(QStringLiteral("本地: 业务服务端连接超时"));
            socket_.abort();
            login_pending_ = false;
            pending_password_.clear();
            state_ = ClientState::Disconnected;
            emit sig_state_changed(state_);
            emit sig_login_failed(QStringLiteral("连接超时，请检查服务端地址和端口"));
        }
    });

    QObject::connect(&socket_, &QTcpSocket::connected, this, [this]() {
        connect_timer_.stop();
        EmitLog(QString("业务服务端: 已连接到 %1:%2")
                    .arg(socket_.peerAddress().toString())
                    .arg(socket_.peerPort()));

        if (!login_pending_) {
            return;
        }

        QJsonObject request;
        request.insert(QStringLiteral("username"), pending_username_);
        request.insert(QStringLiteral("password"), pending_password_);
        SendPacket(gp::protocol::MessageType::kLoginRequest, request);
        EmitLog(QStringLiteral("客户端 >> LoginRequest [账号认证]"));
    });

    QObject::connect(&socket_, &QTcpSocket::readyRead, this, [this]() {
        buffer_.append(socket_.readAll());
        if (buffer_.size() > kMaxBufferSize) {
            EmitLog(QStringLiteral("本地: 业务服务端接收缓冲区溢出，断开连接"));
            buffer_.clear();
            socket_.abort();
            return;
        }
        ProcessBuffer();
    });

    QObject::connect(
        &socket_,
        QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
        this,
        [this](QAbstractSocket::SocketError) {
            connect_timer_.stop();
            const QString error_text = socket_.errorString();
            gp::logging::Logger::Instance().ErrorText(
                QString("业务服务端网络错误: %1").arg(error_text).toUtf8().toStdString());

            if (login_pending_) {
                login_pending_ = false;
                pending_password_.clear();
                state_ = ClientState::Disconnected;
                emit sig_state_changed(state_);
                emit sig_login_failed(error_text);
                return;
            }

            emit sig_business_error(error_text);
        });

    QObject::connect(&socket_, &QTcpSocket::disconnected, this, [this]() {
        connect_timer_.stop();
        buffer_.clear();

        const bool was_login_pending = login_pending_;
        login_pending_ = false;
        pending_password_.clear();
        state_ = ClientState::Disconnected;
        emit sig_state_changed(state_);
        EmitLog(QStringLiteral("业务服务端: 连接已断开"));
        emit sig_connection_closed();

        if (was_login_pending) {
            emit sig_login_failed(QStringLiteral("登录过程中连接已断开"));
        }
    });
}

ClientState BusinessClient::state() const {
    return state_;
}

void BusinessClient::Disconnect() {
    slot_disconnect();
}

void BusinessClient::slot_connect_and_login(
    const ServerInfo& server_info,
    const QString& username,
    const QString& password) {
    if (state_ != ClientState::Disconnected) {
        EmitLog(QStringLiteral("本地: 当前业务连接尚未断开，忽略新的登录请求"));
        return;
    }

    if (server_info.host.trimmed().isEmpty()) {
        emit sig_login_failed(QStringLiteral("业务服务端地址不能为空"));
        return;
    }
    if (username.trimmed().isEmpty() || password.isEmpty()) {
        emit sig_login_failed(QStringLiteral("用户名和密码不能为空"));
        return;
    }

    pending_username_ = username.trimmed();
    pending_password_ = password;
    login_pending_ = true;
    buffer_.clear();
    state_ = ClientState::Connecting;
    emit sig_state_changed(state_);
    EmitLog(QString("本地: 开始连接业务服务端 %1:%2")
                .arg(server_info.host)
                .arg(server_info.port));
    socket_.connectToHost(server_info.host, server_info.port);
    connect_timer_.start(kConnectTimeoutMs);
}

void BusinessClient::slot_disconnect() {
    connect_timer_.stop();
    if (state_ == ClientState::Disconnected) {
        return;
    }

    login_pending_ = false;
    pending_password_.clear();
    if (state_ == ClientState::Connected && socket_.state() == QAbstractSocket::ConnectedState) {
        const QByteArray bytes = gp::qt_protocol::EncodeJsonPacket(gp::protocol::MessageType::kQuitRequest);
        socket_.write(bytes);
        socket_.flush();
        EmitLog(QStringLiteral("客户端 >> QuitRequest"));
    }
    socket_.disconnectFromHost();
}

void BusinessClient::slot_list_patients() {
    if (!EnsureConnected()) {
        return;
    }

    SendPacket(gp::protocol::MessageType::kListPatientsRequest);
    EmitLog(QStringLiteral("客户端 >> ListPatientsRequest"));
}

void BusinessClient::slot_get_patient(const QString& patient_id) {
    if (!EnsureConnected()) {
        return;
    }

    const QString trimmed_id = patient_id.trimmed();
    if (trimmed_id.isEmpty()) {
        emit sig_business_error(QStringLiteral("病人编号不能为空"));
        return;
    }

    QJsonObject request;
    request.insert(QStringLiteral("patient_id"), trimmed_id);
    SendPacket(gp::protocol::MessageType::kGetPatientRequest, request);
    EmitLog(QString("客户端 >> GetPatientRequest %1").arg(trimmed_id));
}

void BusinessClient::slot_add_patient(const PatientInfo& patient_info) {
    if (!EnsureConnected()) {
        return;
    }

    if (patient_info.patient_id.trimmed().isEmpty() || patient_info.name.trimmed().isEmpty()) {
        emit sig_business_error(QStringLiteral("病人编号和姓名不能为空"));
        return;
    }

    QJsonObject request;
    request.insert(QStringLiteral("patient_id"), patient_info.patient_id.trimmed());
    request.insert(QStringLiteral("name"), patient_info.name.trimmed());
    request.insert(QStringLiteral("gender"), patient_info.gender.trimmed());
    request.insert(QStringLiteral("age"), patient_info.age);
    request.insert(QStringLiteral("phone"), patient_info.phone.trimmed());
    request.insert(QStringLiteral("remark"), patient_info.remark.trimmed());
    SendPacket(gp::protocol::MessageType::kAddPatientRequest, request);
    EmitLog(QString("客户端 >> AddPatientRequest %1").arg(patient_info.patient_id.trimmed()));
}

void BusinessClient::slot_update_patient(const PatientInfo& patient_info) {
    if (!EnsureConnected()) {
        return;
    }

    if (patient_info.patient_id.trimmed().isEmpty() || patient_info.name.trimmed().isEmpty()) {
        emit sig_business_error(QStringLiteral("病人编号和姓名不能为空"));
        return;
    }

    QJsonObject request;
    request.insert(QStringLiteral("patient_id"), patient_info.patient_id.trimmed());
    request.insert(QStringLiteral("name"), patient_info.name.trimmed());
    request.insert(QStringLiteral("gender"), patient_info.gender.trimmed());
    request.insert(QStringLiteral("age"), patient_info.age);
    request.insert(QStringLiteral("phone"), patient_info.phone.trimmed());
    request.insert(QStringLiteral("remark"), patient_info.remark.trimmed());
    SendPacket(gp::protocol::MessageType::kUpdatePatientRequest, request);
    EmitLog(QString("客户端 >> UpdatePatientRequest %1").arg(patient_info.patient_id.trimmed()));
}

void BusinessClient::slot_delete_patient(const QString& patient_id) {
    if (!EnsureConnected()) {
        return;
    }

    const QString trimmed_id = patient_id.trimmed();
    if (trimmed_id.isEmpty()) {
        emit sig_business_error(QStringLiteral("请选择要删除的病人"));
        return;
    }

    QJsonObject request;
    request.insert(QStringLiteral("patient_id"), trimmed_id);
    SendPacket(gp::protocol::MessageType::kDeletePatientRequest, request);
    EmitLog(QString("客户端 >> DeletePatientRequest %1").arg(trimmed_id));
}

void BusinessClient::slot_add_monitor_record(const MonitorRecordInfo& record_info) {
    if (!EnsureConnected()) {
        return;
    }

    if (record_info.patient_id.trimmed().isEmpty()) {
        emit sig_business_error(QStringLiteral("病人编号不能为空，无法保存监测记录"));
        return;
    }

    QJsonObject request;
    request.insert(QStringLiteral("patient_id"), record_info.patient_id.trimmed());
    request.insert(QStringLiteral("pred_label"), record_info.pred_label.trimmed());
    request.insert(QStringLiteral("confidence"), record_info.confidence.trimmed());
    request.insert(QStringLiteral("alert_level"), record_info.alert_level.trimmed());
    request.insert(QStringLiteral("latency_ms"), record_info.latency_ms.trimmed());
    request.insert(QStringLiteral("source"), record_info.source.trimmed());
    request.insert(QStringLiteral("sample_name"), record_info.sample_name.trimmed());
    request.insert(QStringLiteral("true_label"), record_info.true_label.trimmed());
    SendPacket(gp::protocol::MessageType::kAddMonitorRecordRequest, request);
    EmitLog(QString("客户端 >> AddMonitorRecordRequest %1 [%2]")
                .arg(record_info.patient_id, record_info.alert_level));
}

void BusinessClient::slot_list_monitor_records(const QString& patient_id) {
    if (!EnsureConnected()) {
        return;
    }

    const QString trimmed_id = patient_id.trimmed();
    if (trimmed_id.isEmpty()) {
        emit sig_business_error(QStringLiteral("病人编号不能为空，无法查询监测记录"));
        return;
    }

    QJsonObject request;
    request.insert(QStringLiteral("patient_id"), trimmed_id);
    SendPacket(gp::protocol::MessageType::kListMonitorRecordsRequest, request);
    EmitLog(QString("客户端 >> ListMonitorRecordsRequest %1").arg(trimmed_id));
}

void BusinessClient::slot_list_all_monitor_records() {
    if (!EnsureConnected()) {
        return;
    }

    SendPacket(gp::protocol::MessageType::kListAllMonitorRecordsRequest);
    EmitLog(QStringLiteral("客户端 >> ListAllMonitorRecordsRequest"));
}

void BusinessClient::slot_list_alerts(const QString& patient_id) {
    if (!EnsureConnected()) {
        return;
    }

    const QString trimmed_id = patient_id.trimmed();
    if (trimmed_id.isEmpty()) {
        emit sig_business_error(QStringLiteral("病人编号不能为空，无法查询报警记录"));
        return;
    }

    QJsonObject request;
    request.insert(QStringLiteral("patient_id"), trimmed_id);
    SendPacket(gp::protocol::MessageType::kListAlertsRequest, request);
    EmitLog(QString("客户端 >> ListAlertsRequest %1").arg(trimmed_id));
}

void BusinessClient::slot_list_all_alerts() {
    if (!EnsureConnected()) {
        return;
    }

    SendPacket(gp::protocol::MessageType::kListAllAlertsRequest);
    EmitLog(QStringLiteral("客户端 >> ListAllAlertsRequest"));
}

void BusinessClient::slot_confirm_alert(const QString& alert_id, const QString& confirmed_by) {
    if (!EnsureConnected()) {
        return;
    }

    const QString trimmed_id = alert_id.trimmed();
    const QString trimmed_by = confirmed_by.trimmed();
    if (trimmed_id.isEmpty()) {
        emit sig_business_error(QStringLiteral("报警编号不能为空，无法确认报警"));
        return;
    }
    if (trimmed_by.isEmpty()) {
        emit sig_business_error(QStringLiteral("确认人不能为空，无法确认报警"));
        return;
    }

    QJsonObject request;
    request.insert(QStringLiteral("alert_id"), trimmed_id);
    request.insert(QStringLiteral("confirmed_by"), trimmed_by);
    SendPacket(gp::protocol::MessageType::kConfirmAlertRequest, request);
    EmitLog(QString("客户端 >> ConfirmAlertRequest %1 [%2]").arg(trimmed_id, trimmed_by));
}

void BusinessClient::slot_change_password(const QString& username, const QString& old_pw, const QString& new_pw) {
    if (!EnsureConnected()) {
        return;
    }

    QJsonObject request;
    request.insert(QStringLiteral("username"), username.trimmed());
    request.insert(QStringLiteral("old_password"), old_pw);
    request.insert(QStringLiteral("new_password"), new_pw);
    SendPacket(gp::protocol::MessageType::kChangePasswordRequest, request);
    EmitLog(QStringLiteral("客户端 >> ChangePasswordRequest"));
}

void BusinessClient::ProcessBuffer() {
    while (true) {
        gp::protocol::Packet packet;
        QString error_text;
        const auto status = gp::qt_protocol::TryTakePacket(&buffer_, &packet, &error_text);
        if (status == gp::qt_protocol::PacketDecodeStatus::kNeedMoreData) {
            break;
        }
        if (status == gp::qt_protocol::PacketDecodeStatus::kError) {
            gp::logging::Logger::Instance().WarningText(error_text.toUtf8().toStdString());
            emit sig_business_error(error_text);
            buffer_.clear();
            socket_.abort();
            return;
        }
        HandlePacket(packet);
    }
}

void BusinessClient::HandlePacket(const gp::protocol::Packet& packet) {
    QJsonObject payload;
    QString error_text;
    if (!gp::qt_protocol::ParseJsonObject(packet, &payload, &error_text)) {
        emit sig_business_error(error_text);
        return;
    }

    EmitLog(QString("业务服务端 << %1").arg(gp::qt_protocol::MessageTypeToText(packet.message_type)));

    switch (packet.message_type) {
    case gp::protocol::MessageType::kByeResponse:
        return;

    case gp::protocol::MessageType::kErrorResponse: {
        const QString text = BuildErrorText(payload);
        gp::logging::Logger::Instance().WarningText(
            QString("业务服务端返回错误: %1").arg(text).toUtf8().toStdString());

        if (login_pending_) {
            login_pending_ = false;
            pending_password_.clear();
            emit sig_login_failed(text);
            socket_.disconnectFromHost();
            return;
        }

        emit sig_business_error(text);
        return;
    }

    case gp::protocol::MessageType::kLoginResponse: {
        login_pending_ = false;
        pending_password_.clear();
        state_ = ClientState::Connected;
        emit sig_state_changed(state_);

        LoginUserInfo user_info;
        user_info.username = JsonValueToText(payload.value(QStringLiteral("username")));
        if (user_info.username.isEmpty()) {
            user_info.username = pending_username_;
        }
        user_info.role = JsonValueToText(payload.value(QStringLiteral("role")));
        user_info.display_name = JsonValueToText(payload.value(QStringLiteral("display_name")));
        emit sig_login_success(user_info);
        return;
    }

    case gp::protocol::MessageType::kListPatientsResponse: {
        QVector<PatientInfo> patients;
        const QJsonArray patient_array = payload.value(QStringLiteral("patients")).toArray();
        patients.reserve(patient_array.size());
        for (const QJsonValue& value : patient_array) {
            if (!value.isObject()) {
                continue;
            }
            PatientInfo patient_info;
            if (ParsePatientObject(value.toObject(), &patient_info)) {
                patients.push_back(patient_info);
            }
        }
        emit sig_patient_list_ready(patients);
        return;
    }

    case gp::protocol::MessageType::kGetPatientResponse: {
        const QJsonObject patient_object = payload.value(QStringLiteral("patient")).toObject();
        PatientInfo patient_info;
        if (!ParsePatientObject(patient_object, &patient_info)) {
            emit sig_business_error(QStringLiteral("无法解析病人详情响应"));
            return;
        }
        emit sig_patient_detail_ready(patient_info);
        return;
    }

    case gp::protocol::MessageType::kAddPatientResponse:
    case gp::protocol::MessageType::kUpdatePatientResponse:
    case gp::protocol::MessageType::kDeletePatientResponse:
        emit sig_patient_operation_success(JsonValueToText(payload.value(QStringLiteral("message"))).trimmed());
        return;

    case gp::protocol::MessageType::kAddMonitorRecordResponse:
        emit sig_monitor_record_saved(JsonValueToText(payload.value(QStringLiteral("message"))).trimmed());
        return;

    case gp::protocol::MessageType::kListMonitorRecordsResponse:
    case gp::protocol::MessageType::kListAllMonitorRecordsResponse: {
        QVector<MonitorRecordInfo> records;
        const QJsonArray record_array = payload.value(QStringLiteral("records")).toArray();
        records.reserve(record_array.size());
        for (const QJsonValue& value : record_array) {
            if (!value.isObject()) {
                continue;
            }
            MonitorRecordInfo record_info;
            if (ParseMonitorRecordObject(value.toObject(), &record_info)) {
                records.push_back(record_info);
            }
        }
        if (packet.message_type == gp::protocol::MessageType::kListMonitorRecordsResponse) {
            emit sig_monitor_records_ready(records);
        } else {
            emit sig_all_monitor_records_ready(records);
        }
        return;
    }

    case gp::protocol::MessageType::kListAlertsResponse:
    case gp::protocol::MessageType::kListAllAlertsResponse: {
        QVector<AlertInfo> alerts;
        const QJsonArray alert_array = payload.value(QStringLiteral("alerts")).toArray();
        alerts.reserve(alert_array.size());
        for (const QJsonValue& value : alert_array) {
            if (!value.isObject()) {
                continue;
            }
            AlertInfo alert_info;
            if (ParseAlertObject(value.toObject(), &alert_info)) {
                alerts.push_back(alert_info);
            }
        }
        if (packet.message_type == gp::protocol::MessageType::kListAlertsResponse) {
            emit sig_alerts_ready(alerts);
        } else {
            emit sig_all_alerts_ready(alerts);
        }
        return;
    }

    case gp::protocol::MessageType::kConfirmAlertResponse:
        emit sig_alert_confirmed(JsonValueToText(payload.value(QStringLiteral("message"))).trimmed());
        return;

    case gp::protocol::MessageType::kChangePasswordResponse:
        emit sig_password_changed(JsonValueToText(payload.value(QStringLiteral("message"))).trimmed());
        return;

    default:
        emit sig_business_error(
            QStringLiteral("未注册的业务响应类型: %1")
                .arg(gp::qt_protocol::MessageTypeToText(packet.message_type)));
        return;
    }
}

void BusinessClient::SendPacket(gp::protocol::MessageType type, const QJsonObject& payload) {
    const QByteArray bytes = gp::qt_protocol::EncodeJsonPacket(type, payload);
    socket_.write(bytes);
}

void BusinessClient::EmitLog(const QString& text) {
    gp::logging::Logger::Instance().InfoText(text.toUtf8().toStdString());
    emit sig_log_message(text);
}

bool BusinessClient::EnsureConnected() {
    if (state_ == ClientState::Connected && socket_.state() == QAbstractSocket::ConnectedState) {
        return true;
    }

    emit sig_business_error(QStringLiteral("业务服务端尚未连接，请重新登录"));
    return false;
}

QString BusinessClient::JsonValueToText(const QJsonValue& value) {
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

bool BusinessClient::ParsePatientObject(const QJsonObject& object, PatientInfo* patient_info) {
    if (patient_info == nullptr) {
        return false;
    }

    bool age_ok = false;
    const int age = JsonValueToText(object.value(QStringLiteral("age"))).toInt(&age_ok);
    if (!age_ok) {
        return false;
    }

    patient_info->patient_id = JsonValueToText(object.value(QStringLiteral("patient_id"))).trimmed();
    patient_info->name = JsonValueToText(object.value(QStringLiteral("name"))).trimmed();
    patient_info->gender = JsonValueToText(object.value(QStringLiteral("gender"))).trimmed();
    patient_info->age = age;
    patient_info->phone = JsonValueToText(object.value(QStringLiteral("phone"))).trimmed();
    patient_info->remark = JsonValueToText(object.value(QStringLiteral("remark"))).trimmed();
    return !patient_info->patient_id.isEmpty();
}

bool BusinessClient::ParseMonitorRecordObject(const QJsonObject& object, MonitorRecordInfo* record_info) {
    if (record_info == nullptr) {
        return false;
    }

    record_info->record_id = JsonValueToText(object.value(QStringLiteral("record_id"))).trimmed();
    record_info->patient_id = JsonValueToText(object.value(QStringLiteral("patient_id"))).trimmed();
    record_info->recorded_at = JsonValueToText(object.value(QStringLiteral("recorded_at"))).trimmed();
    record_info->pred_label = JsonValueToText(object.value(QStringLiteral("pred_label"))).trimmed();
    record_info->confidence = JsonValueToText(object.value(QStringLiteral("confidence"))).trimmed();
    record_info->alert_level = JsonValueToText(object.value(QStringLiteral("alert_level"))).trimmed();
    record_info->latency_ms = JsonValueToText(object.value(QStringLiteral("latency_ms"))).trimmed();
    record_info->source = JsonValueToText(object.value(QStringLiteral("source"))).trimmed();
    record_info->sample_name = JsonValueToText(object.value(QStringLiteral("sample_name"))).trimmed();
    record_info->true_label = JsonValueToText(object.value(QStringLiteral("true_label"))).trimmed();
    return !record_info->record_id.isEmpty();
}

bool BusinessClient::ParseAlertObject(const QJsonObject& object, AlertInfo* alert_info) {
    if (alert_info == nullptr) {
        return false;
    }

    alert_info->alert_id = JsonValueToText(object.value(QStringLiteral("alert_id"))).trimmed();
    alert_info->patient_id = JsonValueToText(object.value(QStringLiteral("patient_id"))).trimmed();
    alert_info->created_at = JsonValueToText(object.value(QStringLiteral("created_at"))).trimmed();
    alert_info->alert_level = JsonValueToText(object.value(QStringLiteral("alert_level"))).trimmed();
    alert_info->pred_label = JsonValueToText(object.value(QStringLiteral("pred_label"))).trimmed();
    alert_info->confidence = JsonValueToText(object.value(QStringLiteral("confidence"))).trimmed();
    alert_info->source = JsonValueToText(object.value(QStringLiteral("source"))).trimmed();
    alert_info->sample_name = JsonValueToText(object.value(QStringLiteral("sample_name"))).trimmed();
    alert_info->status = JsonValueToText(object.value(QStringLiteral("status"))).trimmed();
    if (alert_info->status == QStringLiteral("new")) {
        alert_info->status = QStringLiteral("pending");
    }
    alert_info->confirmed_at = JsonValueToText(object.value(QStringLiteral("confirmed_at"))).trimmed();
    alert_info->confirmed_by = JsonValueToText(object.value(QStringLiteral("confirmed_by"))).trimmed();
    return !alert_info->alert_id.isEmpty();
}

