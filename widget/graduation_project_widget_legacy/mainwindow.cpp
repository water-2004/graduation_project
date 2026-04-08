#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "about_page.h"
#include "alarm_center_page.h"
#include "business_client.h"
#include "dashboard_page.h"
#include "inferpage.h"
#include "login_page.h"
#include "patient_page.h"
#include "sidebar_widget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

namespace {

bool ContainsPatientId(const QVector<PatientInfo>& patients, const QString& patient_id) {
    for (const PatientInfo& patient_info : patients) {
        if (patient_info.patient_id == patient_id) {
            return true;
        }
    }
    return false;
}

void ConfigureStatusDot(QLabel* label, const QString& state_name) {
    if (label == nullptr) {
        return;
    }
    label->setProperty("statusDot", true);
    label->setProperty("connectionState", state_name);
}

void ConfigureStatusText(QLabel* label, const QString& state_name, const QString& text) {
    if (label == nullptr) {
        return;
    }
    label->setText(text);
    label->setProperty("statusText", true);
    label->setProperty("connectionState", state_name);
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);

    business_client_ = new BusinessClient(this);
    login_page_ = new LoginPage(this);
    dashboard_page_ = new DashboardPage(this);
    patient_page_ = new PatientPage(this);
    alarm_center_page_ = new AlarmCenterPage(this);
    infer_page_ = new InferPage(this);
    about_page_ = new AboutPage(this);

    sidebar_ = new SidebarWidget(this);
    sidebar_->AddItem(QStringLiteral("系统概览"));
    sidebar_->AddItem(QStringLiteral("病人管理"));
    sidebar_->AddItem(QStringLiteral("报警中心"));
    sidebar_->AddItem(QStringLiteral("实时监测"));

    stacked_widget_ = new QStackedWidget(this);
    stacked_widget_->setObjectName(QStringLiteral("ContentArea"));
    stacked_widget_->addWidget(dashboard_page_);
    stacked_widget_->addWidget(patient_page_);
    stacked_widget_->addWidget(alarm_center_page_);
    stacked_widget_->addWidget(infer_page_);
    stacked_widget_->addWidget(about_page_);

    top_bar_widget_ = new QWidget(this);
    top_bar_widget_->setObjectName(QStringLiteral("TopBar"));
    auto* top_bar_layout = new QHBoxLayout(top_bar_widget_);
    top_bar_layout->setContentsMargins(24, 16, 24, 16);
    top_bar_layout->setSpacing(16);

    auto* title_layout = new QVBoxLayout();
    title_layout->setSpacing(2);
    top_page_title_label_ = new QLabel(QStringLiteral("实时监测"), top_bar_widget_);
    top_page_title_label_->setObjectName(QStringLiteral("topPageTitle"));
    top_page_subtitle_label_ = new QLabel(QStringLiteral("查看当前监测病人的波形、推理结果与报警状态。"), top_bar_widget_);
    top_page_subtitle_label_->setObjectName(QStringLiteral("topPageSubtitle"));
    title_layout->addWidget(top_page_title_label_);
    title_layout->addWidget(top_page_subtitle_label_);
    top_bar_layout->addLayout(title_layout, 1);

    auto* state_wrap = new QWidget(top_bar_widget_);
    state_wrap->setObjectName(QStringLiteral("topPill"));
    auto* state_layout = new QHBoxLayout(state_wrap);
    state_layout->setContentsMargins(12, 8, 12, 8);
    state_layout->setSpacing(8);
    top_network_dot_label_ = new QLabel(state_wrap);
    top_network_text_label_ = new QLabel(QStringLiteral("业务服务未连接"), state_wrap);
    ConfigureStatusDot(top_network_dot_label_, QStringLiteral("disconnected"));
    ConfigureStatusText(top_network_text_label_, QStringLiteral("disconnected"), QStringLiteral("业务服务未连接"));
    state_layout->addWidget(top_network_dot_label_);
    state_layout->addWidget(top_network_text_label_);
    top_bar_layout->addWidget(state_wrap);

    auto* notice_wrap = new QWidget(top_bar_widget_);
    notice_wrap->setObjectName(QStringLiteral("topPill"));
    auto* notice_layout = new QHBoxLayout(notice_wrap);
    notice_layout->setContentsMargins(12, 8, 12, 8);
    notice_layout->setSpacing(8);
    auto* notice_text_label = new QLabel(QStringLiteral("通知"), notice_wrap);
    top_notice_badge_label_ = new QLabel(QStringLiteral("3"), notice_wrap);
    top_notice_badge_label_->setObjectName(QStringLiteral("noticeBadge"));
    notice_layout->addWidget(notice_text_label);
    notice_layout->addWidget(top_notice_badge_label_);
    top_bar_layout->addWidget(notice_wrap);

    auto* user_wrap = new QWidget(top_bar_widget_);
    user_wrap->setObjectName(QStringLiteral("topUserCard"));
    auto* user_layout = new QHBoxLayout(user_wrap);
    user_layout->setContentsMargins(12, 8, 12, 8);
    user_layout->setSpacing(10);
    auto* avatar_label = new QLabel(QStringLiteral("DR"), user_wrap);
    avatar_label->setObjectName(QStringLiteral("userAvatar"));
    top_user_name_label_ = new QLabel(QStringLiteral("未登录"), user_wrap);
    top_user_name_label_->setObjectName(QStringLiteral("userName"));
    user_layout->addWidget(avatar_label);
    user_layout->addWidget(top_user_name_label_);
    top_bar_layout->addWidget(user_wrap);

    shell_widget_ = new QWidget(this);
    shell_widget_->setObjectName(QStringLiteral("shellRoot"));
    auto* shell_root_layout = new QVBoxLayout(shell_widget_);
    shell_root_layout->setContentsMargins(0, 0, 0, 0);
    shell_root_layout->setSpacing(0);
    shell_root_layout->addWidget(top_bar_widget_);

    auto* shell_body = new QWidget(shell_widget_);
    shell_body->setObjectName(QStringLiteral("shellBody"));
    auto* shell_body_layout = new QHBoxLayout(shell_body);
    shell_body_layout->setContentsMargins(0, 0, 0, 0);
    shell_body_layout->setSpacing(0);
    shell_body_layout->addWidget(sidebar_);
    shell_body_layout->addWidget(stacked_widget_, 1);
    shell_root_layout->addWidget(shell_body, 1);

    root_stacked_widget_ = new QStackedWidget(this);
    root_stacked_widget_->addWidget(login_page_);
    root_stacked_widget_->addWidget(shell_widget_);
    setCentralWidget(root_stacked_widget_);

    ShowLoginEntry();
    UpdateSidebarState(false);
    BindSignals();

    setWindowTitle(QStringLiteral("心电异常检测桌面客户端"));
    resize(1440, 940);

    if (menuBar() != nullptr) {
        menuBar()->hide();
    }
    if (statusBar() != nullptr) {
        statusBar()->hide();
    }
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::OnLoginSuccess(const LoginUserInfo& user_info) {
    current_user_ = user_info;
    infer_page_->ClearCurrentPatient();
    UpdateTopBarUser(user_info);
    UpdateSidebarState(true);
    ShowMainShell();
    SwitchToPage(infer_page_);
}

void MainWindow::OnOpenAlertCenter() {
    ShowMainShell();
    SwitchToPage(alarm_center_page_);
    alarm_center_page_->slot_request_refresh();
}

void MainWindow::OnEnterMonitor(const PatientInfo& patient_info) {
    infer_page_->SetCurrentPatient(patient_info);
    ShowMainShell();
    SwitchToPage(infer_page_);
    setWindowTitle(QStringLiteral("心电异常检测桌面客户端 - %1 - %2")
                       .arg(patient_info.name, patient_info.patient_id));
}

void MainWindow::OnBackToPatients() {
    ShowMainShell();
    SwitchToPage(patient_page_);
}

void MainWindow::OnSidebarIndexChanged(int index) {
    if (index < 0 || index >= stacked_widget_->count()) {
        return;
    }

    QWidget* page = stacked_widget_->widget(index);
    SwitchToPage(page);

    if (page == dashboard_page_) {
        dashboard_page_->slot_request_refresh();
    } else if (page == alarm_center_page_) {
        alarm_center_page_->slot_request_refresh();
    }
}

void MainWindow::OnPatientListReadyForInfer(const QVector<PatientInfo>& patients) {
    if (patients.isEmpty()) {
        infer_page_->ClearCurrentPatient();
        return;
    }

    if (!infer_page_->HasCurrentPatient() ||
        !ContainsPatientId(patients, infer_page_->CurrentPatientId())) {
        infer_page_->SetCurrentPatient(patients.constFirst());
    }
}

void MainWindow::OnBusinessStateChanged(ClientState state) {
    UpdateTopBarConnectionState(state);
}

void MainWindow::OnAboutClicked() {
    ShowMainShell();
    SwitchToPage(about_page_);
}

void MainWindow::OnLogoutClicked() {
    business_client_->Disconnect();
    infer_page_->ClearCurrentPatient();
    current_user_ = LoginUserInfo();
    ResetTopBarUser();
    UpdateTopBarConnectionState(ClientState::Disconnected);
    UpdateSidebarState(false);
    ShowLoginEntry();
}

void MainWindow::ShowLoginEntry() {
    if (root_stacked_widget_ != nullptr) {
        root_stacked_widget_->setCurrentWidget(login_page_);
    }
    setWindowTitle(QStringLiteral("心电异常检测桌面客户端 - 登录"));
}

void MainWindow::ShowMainShell() {
    if (root_stacked_widget_ != nullptr) {
        root_stacked_widget_->setCurrentWidget(shell_widget_);
    }
}

void MainWindow::SwitchToPage(QWidget* page) {
    if (page == nullptr) {
        return;
    }

    stacked_widget_->setCurrentWidget(page);
    const int index = stacked_widget_->indexOf(page);
    if (index >= 0 && index <= kInferPage) {
        sidebar_->SetCurrentIndex(index);
    } else {
        sidebar_->SetCurrentIndex(-1);
    }
    UpdateTopBarForPage(page);
}

void MainWindow::UpdateSidebarState(bool logged_in) {
    sidebar_->SetItemEnabled(kDashboardPage, logged_in);
    sidebar_->SetItemEnabled(kPatientPage, logged_in);
    sidebar_->SetItemEnabled(kAlarmCenterPage, logged_in);
    sidebar_->SetItemEnabled(kInferPage, logged_in);
}

void MainWindow::UpdateTopBarForPage(QWidget* page) {
    QString title = QStringLiteral("实时监测");
    QString subtitle = QStringLiteral("查看当前监测病人的波形、推理结果与报警状态。");

    if (page == dashboard_page_) {
        title = QStringLiteral("系统概览");
        subtitle = QStringLiteral("从病人、记录、报警和模型性能四个维度掌握系统运行全貌。");
    } else if (page == patient_page_) {
        title = QStringLiteral("病人管理");
        subtitle = QStringLiteral("检索、维护病人基础信息，并快速进入实时监测。");
    } else if (page == alarm_center_page_) {
        title = QStringLiteral("报警中心");
        subtitle = QStringLiteral("集中处理未归档告警，查看详情并完成确认闭环。");
    } else if (page == infer_page_) {
        title = QStringLiteral("实时监测");
        subtitle = QStringLiteral("显示心电波形、推理结论与当前告警状态。");
    } else if (page == about_page_) {
        title = QStringLiteral("关于系统");
        subtitle = QStringLiteral("查看连接状态、当前用户与系统版本信息。");
    }

    top_page_title_label_->setText(title);
    top_page_subtitle_label_->setText(subtitle);
    setWindowTitle(QStringLiteral("心电异常检测桌面客户端 - %1").arg(title));
}

void MainWindow::UpdateTopBarConnectionState(ClientState state) {
    if (state == ClientState::Connected) {
        ConfigureStatusDot(top_network_dot_label_, QStringLiteral("connected"));
        ConfigureStatusText(top_network_text_label_, QStringLiteral("connected"), QStringLiteral("业务服务已连接"));
    } else if (state == ClientState::Connecting) {
        ConfigureStatusDot(top_network_dot_label_, QStringLiteral("connecting"));
        ConfigureStatusText(top_network_text_label_, QStringLiteral("connecting"), QStringLiteral("业务服务连接中"));
    } else {
        ConfigureStatusDot(top_network_dot_label_, QStringLiteral("disconnected"));
        ConfigureStatusText(top_network_text_label_, QStringLiteral("disconnected"), QStringLiteral("业务服务未连接"));
    }
    top_network_dot_label_->style()->polish(top_network_dot_label_);
    top_network_text_label_->style()->polish(top_network_text_label_);
}

void MainWindow::UpdateTopBarUser(const LoginUserInfo& user_info) {
    QString text = user_info.display_name;
    if (!user_info.role.isEmpty()) {
        text += QStringLiteral(" · %1").arg(user_info.role);
    }
    top_user_name_label_->setText(text);
}

void MainWindow::ResetTopBarUser() {
    top_user_name_label_->setText(QStringLiteral("未登录"));
}

void MainWindow::BindSignals() {
    connect(sidebar_, &SidebarWidget::sig_index_changed,
            this, &MainWindow::OnSidebarIndexChanged);
    connect(sidebar_, &SidebarWidget::sig_about_clicked,
            this, &MainWindow::OnAboutClicked);
    connect(sidebar_, &SidebarWidget::sig_logout_clicked,
            this, &MainWindow::OnLogoutClicked);

    connect(login_page_, &LoginPage::sig_login_request,
            business_client_, &BusinessClient::slot_connect_and_login);
    connect(login_page_, &LoginPage::sig_login_success_forward,
            this, &MainWindow::OnLoginSuccess);

    connect(business_client_, &BusinessClient::sig_state_changed,
            this, &MainWindow::OnBusinessStateChanged);
    connect(business_client_, &BusinessClient::sig_state_changed,
            login_page_, &LoginPage::slot_on_state_changed);
    connect(business_client_, &BusinessClient::sig_state_changed,
            about_page_, &AboutPage::SetBusinessState);
    connect(business_client_, &BusinessClient::sig_login_success,
            login_page_, &LoginPage::slot_on_login_success);
    connect(business_client_, &BusinessClient::sig_login_success,
            dashboard_page_, &DashboardPage::slot_on_user_ready);
    connect(business_client_, &BusinessClient::sig_login_success,
            patient_page_, &PatientPage::slot_on_user_ready);
    connect(business_client_, &BusinessClient::sig_login_success,
            alarm_center_page_, &AlarmCenterPage::slot_on_user_ready);
    connect(business_client_, &BusinessClient::sig_login_success,
            this, [this](const LoginUserInfo& info) {
                about_page_->SetCurrentUser(info.username);
            });
    connect(business_client_, &BusinessClient::sig_login_failed,
            login_page_, &LoginPage::slot_on_login_failed);

    connect(business_client_, &BusinessClient::sig_log_message,
            login_page_, &LoginPage::slot_append_log);
    connect(business_client_, &BusinessClient::sig_log_message,
            patient_page_, &PatientPage::slot_append_log);
    connect(business_client_, &BusinessClient::sig_log_message,
            alarm_center_page_, &AlarmCenterPage::slot_append_log);
    connect(business_client_, &BusinessClient::sig_log_message,
            infer_page_, &InferPage::slot_append_log);
    connect(business_client_, &BusinessClient::sig_log_message,
            dashboard_page_, &DashboardPage::slot_append_log);

    connect(dashboard_page_, &DashboardPage::sig_load_patients,
            business_client_, &BusinessClient::slot_list_patients);
    connect(dashboard_page_, &DashboardPage::sig_load_all_monitor_records,
            business_client_, &BusinessClient::slot_list_all_monitor_records);
    connect(dashboard_page_, &DashboardPage::sig_load_all_alerts,
            business_client_, &BusinessClient::slot_list_all_alerts);
    connect(business_client_, &BusinessClient::sig_patient_list_ready,
            dashboard_page_, &DashboardPage::slot_on_patient_list_ready);
    connect(business_client_, &BusinessClient::sig_all_monitor_records_ready,
            dashboard_page_, &DashboardPage::slot_on_all_monitor_records_ready);
    connect(business_client_, &BusinessClient::sig_all_alerts_ready,
            dashboard_page_, &DashboardPage::slot_on_all_alerts_ready);
    connect(business_client_, &BusinessClient::sig_connection_closed,
            dashboard_page_, &DashboardPage::slot_on_connection_closed);

    connect(patient_page_, &PatientPage::sig_load_patients,
            business_client_, &BusinessClient::slot_list_patients);
    connect(patient_page_, &PatientPage::sig_add_patient,
            business_client_, &BusinessClient::slot_add_patient);
    connect(patient_page_, &PatientPage::sig_update_patient,
            business_client_, &BusinessClient::slot_update_patient);
    connect(patient_page_, &PatientPage::sig_delete_patient,
            business_client_, &BusinessClient::slot_delete_patient);
    connect(patient_page_, &PatientPage::sig_load_monitor_records,
            business_client_, &BusinessClient::slot_list_monitor_records);
    connect(patient_page_, &PatientPage::sig_load_alerts,
            business_client_, &BusinessClient::slot_list_alerts);
    connect(patient_page_, &PatientPage::sig_confirm_alert,
            business_client_, &BusinessClient::slot_confirm_alert);
    connect(patient_page_, &PatientPage::sig_open_alert_center,
            this, &MainWindow::OnOpenAlertCenter);
    connect(patient_page_, &PatientPage::sig_enter_monitor,
            this, &MainWindow::OnEnterMonitor);

    connect(alarm_center_page_, &AlarmCenterPage::sig_load_patients,
            business_client_, &BusinessClient::slot_list_patients);
    connect(alarm_center_page_, &AlarmCenterPage::sig_load_all_alerts,
            business_client_, &BusinessClient::slot_list_all_alerts);
    connect(alarm_center_page_, &AlarmCenterPage::sig_confirm_alert,
            business_client_, &BusinessClient::slot_confirm_alert);
    connect(alarm_center_page_, &AlarmCenterPage::sig_back_to_patients,
            this, &MainWindow::OnBackToPatients);

    connect(business_client_, &BusinessClient::sig_patient_list_ready,
            this, &MainWindow::OnPatientListReadyForInfer);
    connect(business_client_, &BusinessClient::sig_patient_list_ready,
            patient_page_, &PatientPage::slot_on_patient_list_ready);
    connect(business_client_, &BusinessClient::sig_patient_list_ready,
            alarm_center_page_, &AlarmCenterPage::slot_on_patient_list_ready);
    connect(business_client_, &BusinessClient::sig_patient_detail_ready,
            patient_page_, &PatientPage::slot_on_patient_detail_ready);
    connect(business_client_, &BusinessClient::sig_patient_operation_success,
            patient_page_, &PatientPage::slot_on_patient_operation_success);
    connect(business_client_, &BusinessClient::sig_monitor_records_ready,
            patient_page_, &PatientPage::slot_on_monitor_records_ready);
    connect(business_client_, &BusinessClient::sig_alerts_ready,
            patient_page_, &PatientPage::slot_on_alerts_ready);
    connect(business_client_, &BusinessClient::sig_all_alerts_ready,
            alarm_center_page_, &AlarmCenterPage::slot_on_all_alerts_ready);
    connect(business_client_, &BusinessClient::sig_alert_confirmed,
            patient_page_, &PatientPage::slot_on_alert_confirmed);
    connect(business_client_, &BusinessClient::sig_alert_confirmed,
            alarm_center_page_, &AlarmCenterPage::slot_on_alert_confirmed);
    connect(business_client_, &BusinessClient::sig_business_error,
            patient_page_, &PatientPage::slot_on_business_error);
    connect(business_client_, &BusinessClient::sig_business_error,
            alarm_center_page_, &AlarmCenterPage::slot_on_business_error);
    connect(business_client_, &BusinessClient::sig_business_error,
            dashboard_page_, &DashboardPage::slot_on_business_error);
    connect(business_client_, &BusinessClient::sig_connection_closed,
            patient_page_, &PatientPage::slot_on_connection_closed);
    connect(business_client_, &BusinessClient::sig_connection_closed,
            alarm_center_page_, &AlarmCenterPage::slot_on_connection_closed);

    connect(infer_page_, &InferPage::sig_back_to_patients,
            this, &MainWindow::OnBackToPatients);
    connect(infer_page_, &InferPage::sig_add_monitor_record,
            business_client_, &BusinessClient::slot_add_monitor_record);
    connect(business_client_, &BusinessClient::sig_monitor_record_saved,
            infer_page_, &InferPage::slot_on_monitor_record_saved);
    connect(business_client_, &BusinessClient::sig_business_error,
            infer_page_, &InferPage::slot_on_business_error);
    connect(business_client_, &BusinessClient::sig_connection_closed,
            infer_page_, &InferPage::slot_on_business_connection_closed);

    connect(about_page_, &AboutPage::sig_change_password,
            business_client_, &BusinessClient::slot_change_password);
    connect(business_client_, &BusinessClient::sig_password_changed,
            about_page_, &AboutPage::slot_on_password_changed);
    connect(business_client_, &BusinessClient::sig_business_error,
            about_page_, &AboutPage::slot_on_business_error);

    connect(business_client_, &BusinessClient::sig_connection_closed,
            this, [this]() {
                infer_page_->ClearCurrentPatient();
                current_user_ = LoginUserInfo();
                ResetTopBarUser();
                UpdateTopBarConnectionState(ClientState::Disconnected);
                UpdateSidebarState(false);
                ShowLoginEntry();
            });
}

