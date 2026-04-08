#include "AppViewModel.h"

namespace {

constexpr auto kDefaultMonitorPatientId = "MONITOR_DEMO_001";

QString BusinessStateName(ClientState state) {
    switch (state) {
        case ClientState::Connected:
            return QStringLiteral("connected");
        case ClientState::Connecting:
            return QStringLiteral("connecting");
        case ClientState::Disconnected:
            return QStringLiteral("disconnected");
    }
    return QStringLiteral("disconnected");
}

}  // namespace

AppViewModel::AppViewModel(QObject* parent)
    : QObject(parent)
    , auth_service_(this)
    , network_service_(this)
    , patient_model_(this)
    , alert_manager_(this)
    , dashboard_vm_(this)
    , monitor_vm_(&network_service_, this) {
    connect(&auth_service_, &AuthService::loginSuccess, this, &AppViewModel::HandleLoginSuccess);
    connect(&auth_service_, &AuthService::loginFailed, this, [this](const QString& text) {
        emit toastRequested(text, QStringLiteral("critical"));
    });
    connect(&auth_service_, &AuthService::stateChanged, this, &AppViewModel::HandleBusinessState);
    connect(&auth_service_, &AuthService::connectionClosed, this, [this]() {
        ResetSession();
    });
    connect(&auth_service_, &AuthService::patientListReady, this, [this](const QVector<PatientInfo>& patients) {
        if (!patients.isEmpty()) {
            creating_default_monitor_patient_ = false;
        }
        patient_model_.setPatients(patients);
        dashboard_vm_.setPatients(patients);
        EnsureCurrentPatient();
    });
    connect(&auth_service_,
            &AuthService::allMonitorRecordsReady,
            &dashboard_vm_,
            &DashboardViewModel::setAllRecords);
    connect(&auth_service_, &AuthService::allAlertsReady, this, [this](const QVector<AlertInfo>& alerts) {
        alert_manager_.setAlerts(alerts);
        dashboard_vm_.setAllAlerts(alerts);
        emit alertStatsChanged();
    });
    connect(&auth_service_, &AuthService::businessError, this, [this](const QString& text) {
        emit toastRequested(text, QStringLiteral("critical"));
    });
    connect(&auth_service_, &AuthService::alertConfirmed, this, [this](const QString& text) {
        emit toastRequested(text, QStringLiteral("success"));
        auth_service_.loadAllAlerts();
    });
    connect(&auth_service_, &AuthService::patientOperationSuccess, this, [this](const QString& text) {
        if (!creating_default_monitor_patient_) {
            emit toastRequested(text, QStringLiteral("success"));
        }
        auth_service_.loadPatients();
    });
    connect(&auth_service_, &AuthService::monitorRecordSaved, this, [this](const QString& text) {
        emit toastRequested(text, QStringLiteral("success"));
        auth_service_.loadAllMonitorRecords();
        auth_service_.loadAllAlerts();
    });
    connect(&monitor_vm_, &MonitorViewModel::toastRequested, this, &AppViewModel::toastRequested);
    connect(&monitor_vm_,
            &MonitorViewModel::monitorRecordReady,
            &auth_service_,
            &AuthService::addMonitorRecord);
    connect(alert_manager_.model(), &AlertModel::totalsChanged, this, &AppViewModel::alertStatsChanged);
}

bool AppViewModel::loggedIn() const { return logged_in_; }
int AppViewModel::currentPage() const { return static_cast<int>(current_page_); }

QString AppViewModel::pageTitle() const {
    switch (current_page_) {
        case DashboardPage:
            return QStringLiteral("系统概览");
        case PatientPage:
            return QStringLiteral("病人管理");
        case MonitorPage:
            return QStringLiteral("实时监测");
        case AlertPage:
            return QStringLiteral("报警中心");
        case AboutPage:
            return QStringLiteral("关于系统");
    }
    return QStringLiteral("实时监测");
}

QString AppViewModel::pageSubtitle() const {
    switch (current_page_) {
        case DashboardPage:
            return QStringLiteral("Bento Box 视图，展示病人、记录、报警与模型性能。");
        case PatientPage:
            return QStringLiteral("全宽病人表格与右侧抽屉表单。");
        case MonitorPage:
            return QStringLiteral("沉浸式监测指挥中心，实时查看波形与推理结果。");
        case AlertPage:
            return QStringLiteral("分栏告警处置工作流，左侧待处理列表，右侧详情。");
        case AboutPage:
            return QStringLiteral("查看系统状态、当前用户与连接信息。");
    }
    return QString();
}

QString AppViewModel::currentUserName() const {
    return current_user_.display_name.isEmpty() ? QStringLiteral("未登录") : current_user_.display_name;
}

QString AppViewModel::currentUserRole() const { return current_user_.role; }
QString AppViewModel::businessState() const { return business_state_; }
QString AppViewModel::businessStateText() const { return business_state_text_; }
PatientModel* AppViewModel::patientModel() { return &patient_model_; }
AlertManager* AppViewModel::alertManager() { return &alert_manager_; }
DashboardViewModel* AppViewModel::dashboardViewModel() { return &dashboard_vm_; }
MonitorViewModel* AppViewModel::monitorViewModel() { return &monitor_vm_; }
int AppViewModel::notificationCount() const { return alert_manager_.unconfirmedCount(); }

void AppViewModel::login(const QString& host,
                         int port,
                         const QString& username,
                         const QString& password) {
    auth_service_.login(host, port, username, password);
}

void AppViewModel::logout() {
    network_service_.disconnectEdge();
    auth_service_.logout();
    ResetSession();
}

void AppViewModel::navigate(int page) {
    current_page_ = static_cast<Page>(page);
    emit currentPageChanged();
}

void AppViewModel::refreshAll() {
    if (!logged_in_) {
        return;
    }
    auth_service_.loadPatients();
    auth_service_.loadAllMonitorRecords();
    auth_service_.loadAllAlerts();
}

void AppViewModel::setPatientFilter(const QString& text) {
    patient_model_.setFilterText(text);
}

void AppViewModel::enterMonitorWithPatient(int row) {
    const PatientInfo patient = patient_model_.patientAt(row);
    if (patient.patient_id.isEmpty()) {
        emit toastRequested(QStringLiteral("请选择要进入监测的病人。"), QStringLiteral("warning"));
        return;
    }
    monitor_vm_.setCurrentPatient(patient);
    current_page_ = MonitorPage;
    emit currentPageChanged();
}

QVariantMap AppViewModel::patientFormData(int row) const {
    const PatientInfo patient = patient_model_.patientAt(row);
    return {
        {QStringLiteral("patientId"), patient.patient_id},
        {QStringLiteral("name"), patient.name},
        {QStringLiteral("gender"), patient.gender},
        {QStringLiteral("age"), patient.age},
        {QStringLiteral("phone"), patient.phone},
        {QStringLiteral("remark"), patient.remark},
    };
}

void AppViewModel::savePatient(int row,
                               const QString& patient_id,
                               const QString& name,
                               const QString& gender,
                               int age,
                               const QString& phone,
                               const QString& remark) {
    PatientInfo patient;
    patient.patient_id = patient_id.trimmed();
    patient.name = name.trimmed();
    patient.gender = gender.trimmed();
    patient.age = age;
    patient.phone = phone.trimmed();
    patient.remark = remark.trimmed();

    if (patient.patient_id.isEmpty() || patient.name.isEmpty()) {
        emit toastRequested(QStringLiteral("病人编号和姓名不能为空。"), QStringLiteral("warning"));
        return;
    }

    if (row >= 0) {
        auth_service_.updatePatient(patient);
    } else {
        auth_service_.addPatient(patient);
    }
}

void AppViewModel::deletePatient(int row) {
    const PatientInfo patient = patient_model_.patientAt(row);
    if (patient.patient_id.isEmpty()) {
        emit toastRequested(QStringLiteral("请选择要删除的病人。"), QStringLiteral("warning"));
        return;
    }
    auth_service_.deletePatient(patient.patient_id);
}

void AppViewModel::setAlertKeyword(const QString& text) {
    alert_manager_.model()->setFilterText(text);
}

void AppViewModel::setAlertLevelFilter(const QString& level) {
    alert_manager_.model()->setLevelFilter(level);
}

void AppViewModel::setAlertStatusFilter(const QString& status) {
    alert_manager_.model()->setStatusFilter(status);
}

void AppViewModel::selectAlert(int index) {
    alert_manager_.setSelectedIndex(index);
}

void AppViewModel::confirmSelectedAlert() {
    const AlertInfo alert = alert_manager_.selectedAlertInfo();
    if (alert.alert_id.isEmpty()) {
        emit toastRequested(QStringLiteral("请选择要确认的报警。"), QStringLiteral("warning"));
        return;
    }
    auth_service_.confirmAlert(alert.alert_id, current_user_.username);
}

void AppViewModel::HandleLoginSuccess(const LoginUserInfo& user_info) {
    logged_in_ = true;
    current_user_ = user_info;
    current_page_ = MonitorPage;
    emit loggedInChanged();
    emit userChanged();
    emit currentPageChanged();
    refreshAll();
}

void AppViewModel::HandleBusinessState(ClientState state) {
    business_state_ = BusinessStateName(state);
    if (state == ClientState::Connected) {
        business_state_text_ = QStringLiteral("业务服务已连接");
    } else if (state == ClientState::Connecting) {
        business_state_text_ = QStringLiteral("业务服务连接中");
    } else {
        business_state_text_ = QStringLiteral("业务服务未连接");
    }
    emit businessStateChanged();
}

void AppViewModel::ResetSession() {
    logged_in_ = false;
    creating_default_monitor_patient_ = false;
    current_user_ = LoginUserInfo();
    current_page_ = MonitorPage;
    patient_model_.setPatients({});
    alert_manager_.setAlerts({});
    monitor_vm_.clearCurrentPatient();
    dashboard_vm_.setPatients({});
    dashboard_vm_.setAllRecords({});
    dashboard_vm_.setAllAlerts({});
    emit loggedInChanged();
    emit userChanged();
    emit currentPageChanged();
    emit alertStatsChanged();
}

void AppViewModel::EnsureCurrentPatient() {
    const bool using_default_monitor_patient =
        monitor_vm_.currentPatientId() == QString::fromLatin1(kDefaultMonitorPatientId);
    if (monitor_vm_.hasCurrentPatient() && !using_default_monitor_patient) {
        return;
    }

    const PatientInfo patient = patient_model_.patientAt(0);
    if (!patient.patient_id.isEmpty()) {
        monitor_vm_.setCurrentPatient(patient);
        creating_default_monitor_patient_ = false;
        return;
    }

    if (!logged_in_) {
        return;
    }

    const PatientInfo default_patient = BuildDefaultMonitorPatient();
    monitor_vm_.setCurrentPatient(default_patient);
    if (creating_default_monitor_patient_) {
        return;
    }

    creating_default_monitor_patient_ = true;
    auth_service_.addPatient(default_patient);
}

PatientInfo AppViewModel::BuildDefaultMonitorPatient() const {
    PatientInfo patient;
    patient.patient_id = QString::fromLatin1(kDefaultMonitorPatientId);
    patient.name = QString::fromUtf8(u8"默认监测对象");
    patient.gender = QString::fromUtf8(u8"未知");
    patient.age = 0;
    patient.phone = QStringLiteral("--");
    patient.remark = QString::fromUtf8(u8"系统自动创建的默认监测对象，用于演示实时监测、报警入库与处置流程。");
    return patient;
}
