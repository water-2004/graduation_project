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
class LoginPage;
class PatientPage;
class QStackedWidget;
class SidebarWidget;

// 主窗口只做页面承载与页面切换，不把具体业务逻辑堆在壳层中。
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

private:
    void BindSignals();
    void SwitchToPage(QWidget* page);
    void UpdateSidebarState(bool logged_in);

    Ui::MainWindow* ui;
    SidebarWidget* sidebar_ = nullptr;
    QStackedWidget* stacked_widget_ = nullptr;
    LoginPage* login_page_ = nullptr;
    DashboardPage* dashboard_page_ = nullptr;
    PatientPage* patient_page_ = nullptr;
    AlarmCenterPage* alarm_center_page_ = nullptr;
    InferPage* infer_page_ = nullptr;
    AboutPage* about_page_ = nullptr;
    BusinessClient* business_client_ = nullptr;

    enum PageIndex {
        kLoginPage = 0,
        kDashboardPage,
        kPatientPage,
        kAlarmCenterPage,
        kInferPage,
        kAboutPage,
    };
};

#endif // MAINWINDOW_H

