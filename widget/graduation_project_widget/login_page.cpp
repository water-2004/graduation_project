#include "login_page.h"

#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QString CurrentTimeText() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

}  // namespace

LoginPage::LoginPage(QWidget* parent)
    : QWidget(parent) {
    BuildUi();
    UpdateUiState(ClientState::Disconnected);
}

void LoginPage::slot_on_state_changed(ClientState state) {
    UpdateUiState(state);
}

void LoginPage::slot_on_login_success(const LoginUserInfo& user_info) {
    log_edit_->appendPlainText(
        QStringLiteral("[%1] 登录成功，欢迎 %2")
            .arg(CurrentTimeText(), user_info.display_name));
    emit sig_login_success_forward(user_info);
}

void LoginPage::slot_on_login_failed(const QString& error_text) {
    log_edit_->appendPlainText(QStringLiteral("[%1] 登录失败: %2").arg(CurrentTimeText(), error_text));
    QMessageBox::warning(this, QStringLiteral("登录失败"), error_text);
}

void LoginPage::slot_append_log(const QString& text) {
    log_edit_->appendPlainText(QStringLiteral("[%1] %2").arg(CurrentTimeText(), text));
}

void LoginPage::OnLoginClicked() {
    ServerInfo server_info;
    server_info.host = host_edit_->text().trimmed();
    server_info.port = static_cast<quint16>(port_spin_box_->value());

    const QString username = username_edit_->text().trimmed();
    const QString password = password_edit_->text();
    if (server_info.host.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("登录失败"), QStringLiteral("业务服务端地址不能为空。"));
        return;
    }
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("登录失败"), QStringLiteral("用户名和密码不能为空。"));
        return;
    }

    emit sig_login_request(server_info, username, password);
}

void LoginPage::BuildUi() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(32, 24, 32, 24);
    root_layout->setSpacing(18);

    auto* title_label = new QLabel(QStringLiteral("心电异常检测系统登录"), this);
    title_label->setObjectName(QStringLiteral("pageTitle"));
    title_label->setStyleSheet(QStringLiteral("font-size:28px;"));

    auto* subtitle_label = new QLabel(
        QStringLiteral("先连接业务服务端完成身份认证，登录成功后进入实时监测页面。"), this);
    subtitle_label->setObjectName(QStringLiteral("pageSubtitle"));
    subtitle_label->setWordWrap(true);

    auto* form_group = new QGroupBox(QStringLiteral("登录配置"), this);
    auto* form_layout = new QFormLayout(form_group);
    form_layout->setSpacing(12);

    host_edit_ = new QLineEdit(QStringLiteral("127.0.0.1"), form_group);
    port_spin_box_ = new QSpinBox(form_group);
    port_spin_box_->setRange(1, 65535);
    port_spin_box_->setValue(9200);
    username_edit_ = new QLineEdit(QStringLiteral("admin"), form_group);
    password_edit_ = new QLineEdit(QStringLiteral("123456"), form_group);
    password_edit_->setEchoMode(QLineEdit::Password);
    status_value_label_ = new QLabel(QStringLiteral("未连接"), form_group);

    form_layout->addRow(QStringLiteral("业务服务端地址"), host_edit_);
    form_layout->addRow(QStringLiteral("业务服务端端口"), port_spin_box_);
    form_layout->addRow(QStringLiteral("用户名"), username_edit_);
    form_layout->addRow(QStringLiteral("密码"), password_edit_);
    form_layout->addRow(QStringLiteral("当前状态"), status_value_label_);

    auto* tip_label = new QLabel(
        QStringLiteral("默认管理员账号：admin / 123456。后续病人管理、历史记录等业务功能都将通过该服务端扩展。"),
        this);
    tip_label->setWordWrap(true);
    tip_label->setObjectName(QStringLiteral("tipLabel"));

    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch(1);
    login_button_ = new QPushButton(QStringLiteral("登录系统"), this);
    login_button_->setMinimumHeight(38);
    button_layout->addWidget(login_button_);

    auto* log_group = new QGroupBox(QStringLiteral("登录日志"), this);
    auto* log_layout = new QVBoxLayout(log_group);
    log_edit_ = new QPlainTextEdit(log_group);
    log_edit_->setReadOnly(true);
    log_edit_->setMaximumBlockCount(200);
    log_layout->addWidget(log_edit_);

    root_layout->addWidget(title_label);
    root_layout->addWidget(subtitle_label);
    root_layout->addWidget(form_group);
    root_layout->addWidget(tip_label);
    root_layout->addLayout(button_layout);
    root_layout->addWidget(log_group, 1);

    connect(login_button_, &QPushButton::clicked, this, &LoginPage::OnLoginClicked);
    connect(password_edit_, &QLineEdit::returnPressed, this, &LoginPage::OnLoginClicked);
}

void LoginPage::UpdateUiState(ClientState state) {
    const bool is_connecting = state == ClientState::Connecting;
    const bool is_connected = state == ClientState::Connected;

    host_edit_->setEnabled(!is_connecting && !is_connected);
    port_spin_box_->setEnabled(!is_connecting && !is_connected);
    username_edit_->setEnabled(!is_connecting && !is_connected);
    password_edit_->setEnabled(!is_connecting && !is_connected);
    login_button_->setEnabled(!is_connecting && !is_connected);

    if (is_connected) {
        status_value_label_->setText(QStringLiteral("已登录"));
        status_value_label_->setStyleSheet(QStringLiteral("color:#17663a;font-weight:600;"));
    } else if (is_connecting) {
        status_value_label_->setText(QStringLiteral("连接中"));
        status_value_label_->setStyleSheet(QStringLiteral("color:#b54708;font-weight:600;"));
    } else {
        status_value_label_->setText(QStringLiteral("未连接"));
        status_value_label_->setStyleSheet(QStringLiteral("color:#667085;"));
    }
}
