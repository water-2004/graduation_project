#include "AuthService.h"

AuthService::AuthService(QObject* parent)
    : QObject(parent) {
    connect(&client_, &BusinessClient::sig_state_changed, this, &AuthService::stateChanged);
    connect(&client_, &BusinessClient::sig_log_message, this, &AuthService::logMessage);
    connect(&client_, &BusinessClient::sig_login_success, this, &AuthService::loginSuccess);
    connect(&client_, &BusinessClient::sig_login_failed, this, &AuthService::loginFailed);
    connect(&client_, &BusinessClient::sig_connection_closed, this, &AuthService::connectionClosed);
    connect(&client_, &BusinessClient::sig_patient_list_ready, this, &AuthService::patientListReady);
    connect(&client_, &BusinessClient::sig_all_monitor_records_ready, this, &AuthService::allMonitorRecordsReady);
    connect(&client_, &BusinessClient::sig_all_alerts_ready, this, &AuthService::allAlertsReady);
    connect(&client_, &BusinessClient::sig_business_error, this, &AuthService::businessError);
    connect(&client_, &BusinessClient::sig_alert_confirmed, this, &AuthService::alertConfirmed);
    connect(&client_, &BusinessClient::sig_patient_operation_success, this, &AuthService::patientOperationSuccess);
    connect(&client_, &BusinessClient::sig_monitor_record_saved, this, &AuthService::monitorRecordSaved);
}

ClientState AuthService::state() const {
    return client_.state();
}

void AuthService::login(const QString& host, int port, const QString& username, const QString& password) {
    ServerInfo info;
    info.host = host;
    info.port = static_cast<quint16>(port);
    client_.slot_connect_and_login(info, username, password);
}

void AuthService::logout() {
    client_.slot_disconnect();
}

void AuthService::loadPatients() { client_.slot_list_patients(); }
void AuthService::addPatient(const PatientInfo& patient_info) { client_.slot_add_patient(patient_info); }
void AuthService::updatePatient(const PatientInfo& patient_info) { client_.slot_update_patient(patient_info); }
void AuthService::deletePatient(const QString& patient_id) { client_.slot_delete_patient(patient_id); }
void AuthService::loadAllMonitorRecords() { client_.slot_list_all_monitor_records(); }
void AuthService::loadAllAlerts() { client_.slot_list_all_alerts(); }
void AuthService::confirmAlert(const QString& alert_id, const QString& confirmed_by) { client_.slot_confirm_alert(alert_id, confirmed_by); }
void AuthService::addMonitorRecord(const MonitorRecordInfo& record_info) { client_.slot_add_monitor_record(record_info); }
