#include "business_client.h"

#include <QAbstractSocket>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QStringList>

#include "logger.h"

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
            emit sig_login_failed(QStringLiteral("连接超时，请检查服务器地址和端口"));
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

        const QString login_line = QString("LOGIN %1 %2").arg(pending_username_, pending_password_);
        SendLine(login_line);
        EmitLog(QStringLiteral("客户端 >> LOGIN [账号认证]"));
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
        login_pending_ = false;
        pending_password_.clear();
        state_ = ClientState::Disconnected;
        emit sig_state_changed(state_);
        EmitLog(QStringLiteral("业务服务端: 连接已断开"));
        emit sig_connection_closed();
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
    if (socket_.state() == QAbstractSocket::ConnectedState) {
        socket_.write("QUIT\n");
        socket_.flush();
    }
    socket_.disconnectFromHost();
}

void BusinessClient::slot_list_patients() {
    if (!EnsureConnected()) {
        return;
    }

    SendLine(QStringLiteral("LIST_PATIENTS"));
    EmitLog(QStringLiteral("客户端 >> LIST_PATIENTS"));
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

    SendLine(QString("GET_PATIENT %1").arg(trimmed_id));
    EmitLog(QString("客户端 >> GET_PATIENT %1").arg(trimmed_id));
}

void BusinessClient::slot_add_patient(const PatientInfo& patient_info) {
    if (!EnsureConnected()) {
        return;
    }

    if (patient_info.patient_id.trimmed().isEmpty() || patient_info.name.trimmed().isEmpty()) {
        emit sig_business_error(QStringLiteral("病人编号和姓名不能为空"));
        return;
    }

    const QString payload = QString("%1|%2|%3|%4|%5|%6")
                                .arg(EscapeField(patient_info.patient_id))
                                .arg(EscapeField(patient_info.name))
                                .arg(EscapeField(patient_info.gender))
                                .arg(patient_info.age)
                                .arg(EscapeField(patient_info.phone))
                                .arg(EscapeField(patient_info.remark));
    SendLine(QString("ADD_PATIENT %1").arg(payload));
    EmitLog(QString("客户端 >> ADD_PATIENT %1").arg(patient_info.patient_id.trimmed()));
}

void BusinessClient::slot_update_patient(const PatientInfo& patient_info) {
    if (!EnsureConnected()) {
        return;
    }

    if (patient_info.patient_id.trimmed().isEmpty() || patient_info.name.trimmed().isEmpty()) {
        emit sig_business_error(QStringLiteral("病人编号和姓名不能为空"));
        return;
    }

    const QString payload = QString("%1|%2|%3|%4|%5|%6")
                                .arg(EscapeField(patient_info.patient_id))
                                .arg(EscapeField(patient_info.name))
                                .arg(EscapeField(patient_info.gender))
                                .arg(patient_info.age)
                                .arg(EscapeField(patient_info.phone))
                                .arg(EscapeField(patient_info.remark));
    SendLine(QString("UPDATE_PATIENT %1").arg(payload));
    EmitLog(QString("客户端 >> UPDATE_PATIENT %1").arg(patient_info.patient_id.trimmed()));
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

    SendLine(QString("DELETE_PATIENT %1").arg(trimmed_id));
    EmitLog(QString("客户端 >> DELETE_PATIENT %1").arg(trimmed_id));
}

void BusinessClient::slot_add_monitor_record(const MonitorRecordInfo& record_info) {
    if (!EnsureConnected()) {
        return;
    }

    if (record_info.patient_id.trimmed().isEmpty()) {
        emit sig_business_error(QStringLiteral("病人编号不能为空，无法保存监测记录"));
        return;
    }

    const QString payload = QString("%1|%2|%3|%4|%5|%6|%7|%8")
                                .arg(EscapeField(record_info.patient_id))
                                .arg(EscapeField(record_info.pred_label))
                                .arg(EscapeField(record_info.confidence))
                                .arg(EscapeField(record_info.alert_level))
                                .arg(EscapeField(record_info.latency_ms))
                                .arg(EscapeField(record_info.source))
                                .arg(EscapeField(record_info.sample_name))
                                .arg(EscapeField(record_info.true_label));
    SendLine(QString("ADD_MONITOR_RECORD %1").arg(payload));
    EmitLog(QString("客户端 >> ADD_MONITOR_RECORD %1 [%2]")
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

    SendLine(QString("LIST_MONITOR_RECORDS %1").arg(trimmed_id));
    EmitLog(QString("客户端 >> LIST_MONITOR_RECORDS %1").arg(trimmed_id));
}

void BusinessClient::slot_list_all_monitor_records() {
    if (!EnsureConnected()) {
        return;
    }

    SendLine(QStringLiteral("LIST_ALL_MONITOR_RECORDS"));
    EmitLog(QStringLiteral("客户端 >> LIST_ALL_MONITOR_RECORDS"));
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

    SendLine(QString("LIST_ALERTS %1").arg(trimmed_id));
    EmitLog(QString("客户端 >> LIST_ALERTS %1").arg(trimmed_id));
}

void BusinessClient::slot_list_all_alerts() {
    if (!EnsureConnected()) {
        return;
    }

    SendLine(QStringLiteral("LIST_ALL_ALERTS"));
    EmitLog(QStringLiteral("客户端 >> LIST_ALL_ALERTS"));
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

    SendLine(QString("CONFIRM_ALERT %1|%2").arg(EscapeField(trimmed_id), EscapeField(trimmed_by)));
    EmitLog(QString("客户端 >> CONFIRM_ALERT %1 [%2]").arg(trimmed_id, trimmed_by));
}

void BusinessClient::slot_change_password(const QString& username, const QString& old_pw, const QString& new_pw) {
    if (!EnsureConnected()) {
        return;
    }

    SendLine(QString("CHANGE_PASSWORD %1|%2|%3")
                 .arg(EscapeField(username), EscapeField(old_pw), EscapeField(new_pw)));
    EmitLog(QStringLiteral("客户端 >> CHANGE_PASSWORD"));
}

void BusinessClient::ProcessBuffer() {
    int consumed = 0;
    while (true) {
        const int newline_index = buffer_.indexOf('\n', consumed);
        if (newline_index < 0) {
            break;
        }

        QByteArray line = buffer_.mid(consumed, newline_index - consumed);
        consumed = newline_index + 1;
        if (!line.isEmpty() && line.endsWith('\r')) {
            line.chop(1);
        }
        HandleLine(QString::fromUtf8(line));
    }
    if (consumed > 0) {
        buffer_.remove(0, consumed);
    }
}

void BusinessClient::HandleLine(const QString& line) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    EmitLog(QString("业务服务端 << %1").arg(trimmed));

    if (trimmed.startsWith(QStringLiteral("LOGIN_OK"))) {
        static const QRegularExpression kLoginOkRe(
            R"(^LOGIN_OK\s+role=(\S+)\s+display_name=(.+)$)");
        const QRegularExpressionMatch match = kLoginOkRe.match(trimmed);
        if (!match.hasMatch()) {
            pending_password_.clear();
            emit sig_login_failed(QStringLiteral("无法解析登录响应"));
            return;
        }

        login_pending_ = false;
        pending_password_.clear();
        state_ = ClientState::Connected;
        emit sig_state_changed(state_);

        LoginUserInfo user_info;
        user_info.username = pending_username_;
        user_info.role = match.captured(1);
        user_info.display_name = match.captured(2).trimmed();
        emit sig_login_success(user_info);
        return;
    }

    if (trimmed.startsWith(QStringLiteral("PATIENTS"))) {
        QVector<PatientInfo> patients;
        const QString payload = trimmed.mid(QStringLiteral("PATIENTS").size()).trimmed();
        if (!payload.isEmpty()) {
            const QStringList patient_texts = payload.split(';', Qt::SkipEmptyParts);
            for (const QString& patient_text : patient_texts) {
                PatientInfo patient_info;
                if (ParsePatientText(patient_text, &patient_info)) {
                    patients.push_back(patient_info);
                }
            }
        }
        emit sig_patient_list_ready(patients);
        return;
    }

    if (trimmed.startsWith(QStringLiteral("PATIENT "))) {
        PatientInfo patient_info;
        if (!ParsePatientText(trimmed.mid(QStringLiteral("PATIENT ").size()), &patient_info)) {
            emit sig_business_error(QStringLiteral("无法解析病人详情响应"));
            return;
        }
        emit sig_patient_detail_ready(patient_info);
        return;
    }

    if (trimmed.startsWith(QStringLiteral("ALL_MONITOR_RECORDS"))) {
        QVector<MonitorRecordInfo> records;
        const QString payload = trimmed.mid(QStringLiteral("ALL_MONITOR_RECORDS").size()).trimmed();
        if (!payload.isEmpty()) {
            const QStringList record_texts = payload.split(';', Qt::SkipEmptyParts);
            for (const QString& record_text : record_texts) {
                MonitorRecordInfo record_info;
                if (ParseMonitorRecordText(record_text, &record_info)) {
                    records.push_back(record_info);
                }
            }
        }
        emit sig_all_monitor_records_ready(records);
        return;
    }

    if (trimmed.startsWith(QStringLiteral("MONITOR_RECORDS"))) {
        QVector<MonitorRecordInfo> records;
        const QString payload = trimmed.mid(QStringLiteral("MONITOR_RECORDS").size()).trimmed();
        if (!payload.isEmpty()) {
            const QStringList record_texts = payload.split(';', Qt::SkipEmptyParts);
            for (const QString& record_text : record_texts) {
                MonitorRecordInfo record_info;
                if (ParseMonitorRecordText(record_text, &record_info)) {
                    records.push_back(record_info);
                }
            }
        }
        emit sig_monitor_records_ready(records);
        return;
    }

    if (trimmed.startsWith(QStringLiteral("ALERTS"))) {
        QVector<AlertInfo> alerts;
        const QString payload = trimmed.mid(QStringLiteral("ALERTS").size()).trimmed();
        if (!payload.isEmpty()) {
            const QStringList alert_texts = payload.split(';', Qt::SkipEmptyParts);
            for (const QString& alert_text : alert_texts) {
                AlertInfo alert_info;
                if (ParseAlertText(alert_text, &alert_info)) {
                    alerts.push_back(alert_info);
                }
            }
        }
        emit sig_alerts_ready(alerts);
        return;
    }

    if (trimmed.startsWith(QStringLiteral("ALL_ALERTS"))) {
        QVector<AlertInfo> alerts;
        const QString payload = trimmed.mid(QStringLiteral("ALL_ALERTS").size()).trimmed();
        if (!payload.isEmpty()) {
            const QStringList alert_texts = payload.split(';', Qt::SkipEmptyParts);
            for (const QString& alert_text : alert_texts) {
                AlertInfo alert_info;
                if (ParseAlertText(alert_text, &alert_info)) {
                    alerts.push_back(alert_info);
                }
            }
        }
        emit sig_all_alerts_ready(alerts);
        return;
    }

    if (trimmed.startsWith(QStringLiteral("RECORD_OK"))) {
        emit sig_monitor_record_saved(trimmed.mid(QStringLiteral("RECORD_OK").size()).trimmed());
        return;
    }

    if (trimmed.startsWith(QStringLiteral("ALERT_OK"))) {
        emit sig_alert_confirmed(trimmed.mid(QStringLiteral("ALERT_OK").size()).trimmed());
        return;
    }

    if (trimmed.startsWith(QStringLiteral("PASSWORD_OK"))) {
        emit sig_password_changed(trimmed.mid(QStringLiteral("PASSWORD_OK").size()).trimmed());
        return;
    }

    if (trimmed.startsWith(QStringLiteral("OK"))) {
        emit sig_patient_operation_success(trimmed.mid(2).trimmed());
        return;
    }

    if (trimmed.startsWith(QStringLiteral("ERR"))) {
        const QString error_text = trimmed.mid(3).trimmed();
        gp::logging::Logger::Instance().WarningText(
            QString("业务服务端返回错误: %1").arg(error_text).toUtf8().toStdString());

        if (login_pending_) {
            login_pending_ = false;
            pending_password_.clear();
            emit sig_login_failed(error_text);
            socket_.disconnectFromHost();
            return;
        }

        emit sig_business_error(error_text);
    }
}

void BusinessClient::SendLine(const QString& line) {
    socket_.write(line.toUtf8());
    socket_.write("\n");
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

bool BusinessClient::ParsePatientText(const QString& text, PatientInfo* patient_info) {
    if (patient_info == nullptr) {
        return false;
    }

    const QStringList fields = text.split('|', Qt::KeepEmptyParts);
    if (fields.size() < 6) {
        return false;
    }

    bool age_ok = false;
    const int age = fields[3].trimmed().toInt(&age_ok);
    if (!age_ok) {
        return false;
    }

    patient_info->patient_id = fields[0].trimmed();
    patient_info->name = fields[1].trimmed();
    patient_info->gender = fields[2].trimmed();
    patient_info->age = age;
    patient_info->phone = fields[4].trimmed();
    patient_info->remark = fields.mid(5).join("|").trimmed();
    return !patient_info->patient_id.isEmpty();
}

bool BusinessClient::ParseMonitorRecordText(const QString& text, MonitorRecordInfo* record_info) {
    if (record_info == nullptr) {
        return false;
    }

    const QStringList fields = text.split('|', Qt::KeepEmptyParts);
    if (fields.size() < 10) {
        return false;
    }

    record_info->record_id = fields[0].trimmed();
    record_info->patient_id = fields[1].trimmed();
    record_info->recorded_at = fields[2].trimmed();
    record_info->pred_label = fields[3].trimmed();
    record_info->confidence = fields[4].trimmed();
    record_info->alert_level = fields[5].trimmed();
    record_info->latency_ms = fields[6].trimmed();
    record_info->source = fields[7].trimmed();
    record_info->sample_name = fields[8].trimmed();
    record_info->true_label = fields.mid(9).join("|").trimmed();
    return !record_info->record_id.isEmpty();
}

bool BusinessClient::ParseAlertText(const QString& text, AlertInfo* alert_info) {
    if (alert_info == nullptr) {
        return false;
    }

    const QStringList fields = text.split('|', Qt::KeepEmptyParts);
    if (fields.size() < 9) {
        return false;
    }

    alert_info->alert_id = fields[0].trimmed();
    alert_info->patient_id = fields[1].trimmed();
    alert_info->created_at = fields[2].trimmed();
    alert_info->alert_level = fields[3].trimmed();
    alert_info->pred_label = fields[4].trimmed();
    alert_info->confidence = fields[5].trimmed();
    alert_info->source = fields[6].trimmed();
    alert_info->sample_name = fields[7].trimmed();
    alert_info->status = fields[8].trimmed();
    if (alert_info->status == QStringLiteral("new")) {
        alert_info->status = QStringLiteral("pending");
    }
    if (fields.size() >= 11) {
        alert_info->confirmed_at = fields[9].trimmed();
        alert_info->confirmed_by = fields[10].trimmed();
    }
    return !alert_info->alert_id.isEmpty();
}

QString BusinessClient::EscapeField(const QString& text) {
    QString sanitized = text;
    sanitized.replace('|', '/');
    sanitized.replace(';', ' ');
    sanitized.replace('\r', ' ');
    sanitized.replace('\n', ' ');
    return sanitized.trimmed();
}

