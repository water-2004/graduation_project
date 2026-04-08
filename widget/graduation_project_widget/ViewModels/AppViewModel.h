#ifndef GP_QML_APP_VIEW_MODEL_H_
#define GP_QML_APP_VIEW_MODEL_H_

#include <QObject>

#include "../Backend/AlertManager.h"
#include "../Backend/AuthService.h"
#include "../Backend/NetworkService.h"
#include "../Models/PatientModel.h"
#include "DashboardViewModel.h"
#include "MonitorViewModel.h"

class AppViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(int currentPage READ currentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QString pageTitle READ pageTitle NOTIFY currentPageChanged)
    Q_PROPERTY(QString pageSubtitle READ pageSubtitle NOTIFY currentPageChanged)
    Q_PROPERTY(QString currentUserName READ currentUserName NOTIFY userChanged)
    Q_PROPERTY(QString currentUserRole READ currentUserRole NOTIFY userChanged)
    Q_PROPERTY(QString businessState READ businessState NOTIFY businessStateChanged)
    Q_PROPERTY(QString businessStateText READ businessStateText NOTIFY businessStateChanged)
    Q_PROPERTY(PatientModel* patientModel READ patientModel CONSTANT)
    Q_PROPERTY(AlertManager* alertManager READ alertManager CONSTANT)
    Q_PROPERTY(DashboardViewModel* dashboardViewModel READ dashboardViewModel CONSTANT)
    Q_PROPERTY(MonitorViewModel* monitorViewModel READ monitorViewModel CONSTANT)
    Q_PROPERTY(int notificationCount READ notificationCount NOTIFY alertStatsChanged)

public:
    enum Page {
        DashboardPage = 0,
        PatientPage,
        MonitorPage,
        AlertPage,
        AboutPage,
    };
    Q_ENUM(Page)

    explicit AppViewModel(QObject* parent = nullptr);

    bool loggedIn() const;
    int currentPage() const;
    QString pageTitle() const;
    QString pageSubtitle() const;
    QString currentUserName() const;
    QString currentUserRole() const;
    QString businessState() const;
    QString businessStateText() const;
    PatientModel* patientModel();
    AlertManager* alertManager();
    DashboardViewModel* dashboardViewModel();
    MonitorViewModel* monitorViewModel();
    int notificationCount() const;

    Q_INVOKABLE void login(const QString& host, int port, const QString& username, const QString& password);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void navigate(int page);
    Q_INVOKABLE void refreshAll();
    Q_INVOKABLE void setPatientFilter(const QString& text);
    Q_INVOKABLE void enterMonitorWithPatient(int row);
    Q_INVOKABLE QVariantMap patientFormData(int row) const;
    Q_INVOKABLE void savePatient(int row,
                                 const QString& patient_id,
                                 const QString& name,
                                 const QString& gender,
                                 int age,
                                 const QString& phone,
                                 const QString& remark);
    Q_INVOKABLE void deletePatient(int row);
    Q_INVOKABLE void setAlertKeyword(const QString& text);
    Q_INVOKABLE void setAlertLevelFilter(const QString& level);
    Q_INVOKABLE void setAlertStatusFilter(const QString& status);
    Q_INVOKABLE void selectAlert(int index);
    Q_INVOKABLE void confirmSelectedAlert();

signals:
    void loggedInChanged();
    void currentPageChanged();
    void userChanged();
    void businessStateChanged();
    void alertStatsChanged();
    void toastRequested(const QString& message, const QString& level);

private:
    void HandleLoginSuccess(const LoginUserInfo& user_info);
    void HandleBusinessState(ClientState state);
    void ResetSession();
    void EnsureCurrentPatient();
    PatientInfo BuildDefaultMonitorPatient() const;

    AuthService auth_service_;
    NetworkService network_service_;
    PatientModel patient_model_;
    AlertManager alert_manager_;
    DashboardViewModel dashboard_vm_;
    MonitorViewModel monitor_vm_;
    bool logged_in_ = false;
    Page current_page_ = MonitorPage;
    LoginUserInfo current_user_;
    QString business_state_ = QStringLiteral("disconnected");
    QString business_state_text_ = QStringLiteral("业务服务未连接");
    bool creating_default_monitor_patient_ = false;
};

#endif  // GP_QML_APP_VIEW_MODEL_H_
