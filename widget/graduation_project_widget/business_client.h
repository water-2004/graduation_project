#ifndef BUSINESS_CLIENT_H
#define BUSINESS_CLIENT_H

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QVector>

#include "global.h"
#include "protocol/message_protocol.h"

// 业务服务端客户端：负责登录链路、病人管理链路和业务记录链路。
class BusinessClient : public QObject {
    Q_OBJECT

public:
    explicit BusinessClient(QObject* parent = nullptr);

    ClientState state() const;
    void Disconnect();

public slots:
    void slot_connect_and_login(const ServerInfo& server_info,
                                const QString& username,
                                const QString& password);
    void slot_disconnect();
    void slot_list_patients();
    void slot_get_patient(const QString& patient_id);
    void slot_add_patient(const PatientInfo& patient_info);
    void slot_update_patient(const PatientInfo& patient_info);
    void slot_delete_patient(const QString& patient_id);
    void slot_add_monitor_record(const MonitorRecordInfo& record_info);
    void slot_list_monitor_records(const QString& patient_id);
    void slot_list_all_monitor_records();
    void slot_list_alerts(const QString& patient_id);
    void slot_list_all_alerts();
    void slot_confirm_alert(const QString& alert_id, const QString& confirmed_by);
    void slot_change_password(const QString& username, const QString& old_pw, const QString& new_pw);

signals:
    void sig_login_success(const LoginUserInfo& user_info);
    void sig_login_failed(const QString& error_text);
    void sig_state_changed(ClientState state);
    void sig_log_message(const QString& text);
    void sig_connection_closed();

    void sig_patient_list_ready(const QVector<PatientInfo>& patients);
    void sig_patient_detail_ready(const PatientInfo& patient_info);
    void sig_patient_operation_success(const QString& message);

    void sig_monitor_record_saved(const QString& message);
    void sig_monitor_records_ready(const QVector<MonitorRecordInfo>& records);
    void sig_all_monitor_records_ready(const QVector<MonitorRecordInfo>& records);
    void sig_alerts_ready(const QVector<AlertInfo>& alerts);
    void sig_all_alerts_ready(const QVector<AlertInfo>& alerts);
    void sig_alert_confirmed(const QString& message);
    void sig_password_changed(const QString& message);

    void sig_business_error(const QString& error_text);

private:
    void ProcessBuffer();
    void HandlePacket(const gp::protocol::Packet& packet);
    void SendPacket(gp::protocol::MessageType type, const QJsonObject& payload = QJsonObject());
    void EmitLog(const QString& text);
    bool EnsureConnected();

    static QString JsonValueToText(const QJsonValue& value);
    static bool ParsePatientObject(const QJsonObject& object, PatientInfo* patient_info);
    static bool ParseMonitorRecordObject(const QJsonObject& object, MonitorRecordInfo* record_info);
    static bool ParseAlertObject(const QJsonObject& object, AlertInfo* alert_info);

    static constexpr int kConnectTimeoutMs = 10000;
    static constexpr int kMaxBufferSize = 10 * 1024 * 1024;

    QTcpSocket socket_;
    QByteArray buffer_;
    QTimer connect_timer_;
    ClientState state_ = ClientState::Disconnected;
    QString pending_username_;
    QString pending_password_;
    bool login_pending_ = false;
};

#endif  // BUSINESS_CLIENT_H

