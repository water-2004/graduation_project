#ifndef GP_QML_AUTH_SERVICE_H_
#define GP_QML_AUTH_SERVICE_H_

#include <QObject>

#include "../business_client.h"

class AuthService : public QObject {
    Q_OBJECT

public:
    explicit AuthService(QObject* parent = nullptr);

    ClientState state() const;

public slots:
    void login(const QString& host, int port, const QString& username, const QString& password);
    void logout();
    void loadPatients();
    void addPatient(const PatientInfo& patient_info);
    void updatePatient(const PatientInfo& patient_info);
    void deletePatient(const QString& patient_id);
    void loadAllMonitorRecords();
    void loadAllAlerts();
    void confirmAlert(const QString& alert_id, const QString& confirmed_by);
    void addMonitorRecord(const MonitorRecordInfo& record_info);

signals:
    void stateChanged(ClientState state);
    void logMessage(const QString& text);
    void loginSuccess(const LoginUserInfo& user_info);
    void loginFailed(const QString& error_text);
    void connectionClosed();
    void patientListReady(const QVector<PatientInfo>& patients);
    void allMonitorRecordsReady(const QVector<MonitorRecordInfo>& records);
    void allAlertsReady(const QVector<AlertInfo>& alerts);
    void businessError(const QString& error_text);
    void alertConfirmed(const QString& message);
    void patientOperationSuccess(const QString& message);
    void monitorRecordSaved(const QString& message);

private:
    BusinessClient client_;
};

#endif  // GP_QML_AUTH_SERVICE_H_
