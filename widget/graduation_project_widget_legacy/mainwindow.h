#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "global.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class AboutPage;
class AlarmCenterPage;
class BusinessClient;
class DashboardPage;
class InferPage;
class QLabel;
class LoginPage;
class PatientPage;
class QStackedWidget;
class SidebarWidget;
class QWidget;

// 主窗口只负责页面容器、顶栏状态和导航控制，不承载具体业务逻辑。
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void OnLoginSuccess(const LoginUserInfo& user_info);
    void OnOpenAlertCenter();
    void OnEnterMonitor(const PatientInfo& patient_info);
    void OnBackToPatients();
    void OnSidebarIndexChanged(int index);
    void OnPatientListReadyForInfer(const QVector<PatientInfo>& patients);
    void OnBusinessStateChanged(ClientState state);
    void OnAboutClicked();
    void OnLogoutClicked();

private:
    void BindSignals();
    void ShowLoginEntry();
    void ShowMainShell();
    void SwitchToPage(QWidget* page);
    void UpdateSidebarState(bool logged_in);
    void UpdateTopBarForPage(QWidget* page);
    void UpdateTopBarConnectionState(ClientState state);
    void UpdateTopBarUser(const LoginUserInfo& user_info);
    void ResetTopBarUser();

    Ui::MainWindow* ui;
    QStackedWidget* root_stacked_widget_ = nullptr;
    QWidget* shell_widget_ = nullptr;
    QWidget* top_bar_widget_ = nullptr;
    SidebarWidget* sidebar_ = nullptr;
    QStackedWidget* stacked_widget_ = nullptr;
    QLabel* top_page_title_label_ = nullptr;
    QLabel* top_page_subtitle_label_ = nullptr;
    QLabel* top_network_dot_label_ = nullptr;
    QLabel* top_network_text_label_ = nullptr;
    QLabel* top_notice_badge_label_ = nullptr;
    QLabel* top_user_name_label_ = nullptr;
    LoginPage* login_page_ = nullptr;
    DashboardPage* dashboard_page_ = nullptr;
    PatientPage* patient_page_ = nullptr;
    AlarmCenterPage* alarm_center_page_ = nullptr;
    InferPage* infer_page_ = nullptr;
    AboutPage* about_page_ = nullptr;
    BusinessClient* business_client_ = nullptr;
    LoginUserInfo current_user_;

    enum PageIndex {
        kDashboardPage = 0,
        kPatientPage,
        kAlarmCenterPage,
        kInferPage,
    };
};

#endif // MAINWINDOW_H
