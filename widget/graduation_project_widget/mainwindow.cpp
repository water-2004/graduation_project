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
#include <QMenuBar>
#include <QStackedWidget>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    business_client_ = new BusinessClient(this);
    login_page_ = new LoginPage(this);
    dashboard_page_ = new DashboardPage(this);
    patient_page_ = new PatientPage(this);
    alarm_center_page_ = new AlarmCenterPage(this);
    infer_page_ = new InferPage(this);
    about_page_ = new AboutPage(this);

    sidebar_ = new SidebarWidget(this);
    sidebar_->AddItem(QStringLiteral("登录"));        // 0
    sidebar_->AddItem(QStringLiteral("系统概览"));    // 1
    sidebar_->AddItem(QStringLiteral("病人管理"));    // 2
    sidebar_->AddItem(QStringLiteral("报警中心"));    // 3
    sidebar_->AddItem(QStringLiteral("实时监测"));    // 4
    sidebar_->AddItem(QStringLiteral("关于系统"));    // 5

    stacked_widget_ = new QStackedWidget(this);
    stacked_widget_->addWidget(login_page_);          // 0
    stacked_widget_->addWidget(dashboard_page_);      // 1
    stacked_widget_->addWidget(patient_page_);        // 2
    stacked_widget_->addWidget(alarm_center_page_);   // 3
    stacked_widget_->addWidget(infer_page_);          // 4
    stacked_widget_->addWidget(about_page_);          // 5

    auto* central = new QWidget(this);
    auto* h_layout = new QHBoxLayout(central);
    h_layout->setContentsMargins(0, 0, 0, 0);
    h_layout->setSpacing(0);
    h_layout->addWidget(sidebar_);
    h_layout->addWidget(stacked_widget_, 1);
    setCentralWidget(central);

    SwitchToPage(login_page_);
    UpdateSidebarState(false);

    BindSignals();

    setWindowTitle(QStringLiteral("心电异常检测桌面客户端"));
    resize(1240, 900);

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
    UpdateSidebarState(true);
    SwitchToPage(dashboard_page_);
    dashboard_page_->slot_request_refresh();
    setWindowTitle(QStringLiteral("心电异常检测桌面客户端 - %1").arg(user_info.display_name));
}

void MainWindow::OnOpenAlertCenter() {
    SwitchToPage(alarm_center_page_);
    alarm_center_page_->slot_request_refresh();
}

void MainWindow::OnEnterMonitor(const PatientInfo& patient_info) {
    infer_page_->SetCurrentPatient(patient_info);
    SwitchToPage(infer_page_);
    setWindowTitle(QStringLiteral("心电异常检测桌面客户端 - %1 - %2")
                       .arg(patient_info.name, patient_info.patient_id));
}

void MainWindow::OnBackToPatients() {
    SwitchToPage(patient_page_);
}

void MainWindow::OnSidebarIndexChanged(int index) {
    if (index >= 0 && index < stacked_widget_->count()) {
        QWidget* page = stacked_widget_->widget(index);
        SwitchToPage(page);

        if (page == dashboard_page_) {
            dashboard_page_->slot_request_refresh();
        } else if (page == alarm_center_page_) {
            alarm_center_page_->slot_request_refresh();
        }
    }
}

void MainWindow::SwitchToPage(QWidget* page) {
    stacked_widget_->setCurrentWidget(page);
    const int index = stacked_widget_->indexOf(page);
    sidebar_->SetCurrentIndex(index);

    static const QString titles[] = {
        QStringLiteral("登录"),
        QStringLiteral("系统概览"),
        QStringLiteral("病人管理"),
        QStringLiteral("报警中心"),
        QStringLiteral("实时监测"),
        QStringLiteral("关于系统"),
    };
    if (index >= 0 && index < static_cast<int>(std::size(titles))) {
        setWindowTitle(QStringLiteral("心电异常检测桌面客户端 - %1").arg(titles[index]));
    }
}

void MainWindow::UpdateSidebarState(bool logged_in) {
    sidebar_->SetItemEnabled(kLoginPage, !logged_in);
    sidebar_->SetItemEnabled(kDashboardPage, logged_in);
    sidebar_->SetItemEnabled(kPatientPage, logged_in);
    sidebar_->SetItemEnabled(kAlarmCenterPage, logged_in);
    sidebar_->SetItemEnabled(kInferPage, logged_in);
    sidebar_->SetItemEnabled(kAboutPage, true);
}

void MainWindow::BindSignals() {
    connect(sidebar_, &SidebarWidget::sig_index_changed,
            this, &MainWindow::OnSidebarIndexChanged);

    connect(login_page_, &LoginPage::sig_login_request,
            business_client_, &BusinessClient::slot_connect_and_login);
    connect(login_page_, &LoginPage::sig_login_success_forward,
            this, &MainWindow::OnLoginSuccess);

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

    // Dashboard signals
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

    // Patient page signals
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

    // Alarm center signals
    connect(alarm_center_page_, &AlarmCenterPage::sig_load_patients,
            business_client_, &BusinessClient::slot_list_patients);
    connect(alarm_center_page_, &AlarmCenterPage::sig_load_all_alerts,
            business_client_, &BusinessClient::slot_list_all_alerts);
    connect(alarm_center_page_, &AlarmCenterPage::sig_confirm_alert,
            business_client_, &BusinessClient::slot_confirm_alert);
    connect(alarm_center_page_, &AlarmCenterPage::sig_back_to_patients,
            this, &MainWindow::OnBackToPatients);

    // Business client -> pages
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

    // Infer page signals
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

    // About page: password change
    connect(about_page_, &AboutPage::sig_change_password,
            business_client_, &BusinessClient::slot_change_password);
    connect(business_client_, &BusinessClient::sig_password_changed,
            about_page_, &AboutPage::slot_on_password_changed);
    connect(business_client_, &BusinessClient::sig_business_error,
            about_page_, &AboutPage::slot_on_business_error);

    // Connection closed -> reset sidebar
    connect(business_client_, &BusinessClient::sig_connection_closed,
            this, [this]() {
                UpdateSidebarState(false);
                SwitchToPage(login_page_);
            });
}
