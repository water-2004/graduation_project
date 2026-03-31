#ifndef LOGIN_PAGE_H
#define LOGIN_PAGE_H

#include <QWidget>

#include "global.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QPlainTextEdit;

// 登录页：负责业务服务端连接与账号认证，不直接处理监测逻辑。
class LoginPage : public QWidget {
    Q_OBJECT

public:
    explicit LoginPage(QWidget* parent = nullptr);

signals:
    void sig_login_request(const ServerInfo& server_info,
                           const QString& username,
                           const QString& password);
    void sig_login_success_forward(const LoginUserInfo& user_info);

public slots:
    void slot_on_state_changed(ClientState state);
    void slot_on_login_success(const LoginUserInfo& user_info);
    void slot_on_login_failed(const QString& error_text);
    void slot_append_log(const QString& text);

private slots:
    void OnLoginClicked();

private:
    void BuildUi();
    void UpdateUiState(ClientState state);

    QLabel* status_value_label_ = nullptr;
    QLineEdit* host_edit_ = nullptr;
    QSpinBox* port_spin_box_ = nullptr;
    QLineEdit* username_edit_ = nullptr;
    QLineEdit* password_edit_ = nullptr;
    QPushButton* login_button_ = nullptr;
    QPlainTextEdit* log_edit_ = nullptr;
};

#endif // LOGIN_PAGE_H
