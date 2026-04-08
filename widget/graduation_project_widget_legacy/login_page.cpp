#include "login_page.h"

#include <QDateTime>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QString CurrentTimeText() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

QString ConnectionStateName(ClientState state) {
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

void RefreshWidgetStyle(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
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
    log_edit_->appendPlainText(QStringLiteral("[%1] 登录失败：%2").arg(CurrentTimeText(), error_text));
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
        QMessageBox::warning(this, QStringLiteral("登录失败"), QStringLiteral("业务服务地址不能为空。"));
        return;
    }
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("登录失败"), QStringLiteral("用户名和密码不能为空。"));
        return;
    }

    emit sig_login_request(server_info, username, password);
}

void LoginPage::BuildUi() {
    auto* root_layout = new QHBoxLayout(this);
    root_layout->setContentsMargins(48, 36, 48, 36);
    root_layout->setSpacing(28);

    auto* hero_frame = new QFrame(this);
    hero_frame->setObjectName(QStringLiteral("loginHero"));
    auto* hero_layout = new QVBoxLayout(hero_frame);
    hero_layout->setContentsMargins(36, 36, 36, 36);
    hero_layout->setSpacing(18);

    auto* hero_eyebrow = new QLabel(QStringLiteral("智慧医疗 · ECG Edge AI"), hero_frame);
    hero_eyebrow->setObjectName(QStringLiteral("loginHeroEyebrow"));
    auto* hero_title = new QLabel(QStringLiteral("心电异常监护桌面端"), hero_frame);
    hero_title->setObjectName(QStringLiteral("loginHeroTitle"));
    hero_title->setWordWrap(true);
    auto* hero_text = new QLabel(
        QStringLiteral("连接业务服务与边缘推理服务后，系统可直接展示当前监测对象的实时波形、推理结果与告警状态。"),
        hero_frame);
    hero_text->setObjectName(QStringLiteral("loginHeroText"));
    hero_text->setWordWrap(true);

    auto* hero_feature_1 = new QLabel(QStringLiteral("01  实时心电波形与异常检测联动展示"), hero_frame);
    auto* hero_feature_2 = new QLabel(QStringLiteral("02  支持病人记录、报警归档与历史回放"), hero_frame);
    auto* hero_feature_3 = new QLabel(QStringLiteral("03  面向边缘设备部署的轻量化诊断链路"), hero_frame);
    hero_feature_1->setObjectName(QStringLiteral("loginHeroFeature"));
    hero_feature_2->setObjectName(QStringLiteral("loginHeroFeature"));
    hero_feature_3->setObjectName(QStringLiteral("loginHeroFeature"));

    auto* hero_tip = new QLabel(QStringLiteral("建议流程：登录后先进入实时监测页，再查看系统概览、病人管理和报警中心。"), hero_frame);
    hero_tip->setObjectName(QStringLiteral("loginHeroTip"));
    hero_tip->setWordWrap(true);

    hero_layout->addWidget(hero_eyebrow);
    hero_layout->addWidget(hero_title);
    hero_layout->addWidget(hero_text);
    hero_layout->addSpacing(10);
    hero_layout->addWidget(hero_feature_1);
    hero_layout->addWidget(hero_feature_2);
    hero_layout->addWidget(hero_feature_3);
    hero_layout->addStretch();
    hero_layout->addWidget(hero_tip);

    auto* card_frame = new QFrame(this);
    card_frame->setObjectName(QStringLiteral("loginCard"));
    card_frame->setMinimumWidth(460);
    auto* card_layout = new QVBoxLayout(card_frame);
    card_layout->setContentsMargins(32, 32, 32, 32);
    card_layout->setSpacing(18);

    auto* card_title = new QLabel(QStringLiteral("登录系统"), card_frame);
    card_title->setObjectName(QStringLiteral("pageTitle"));
    auto* card_subtitle = new QLabel(
        QStringLiteral("请输入业务服务连接信息和账号密码。登录成功后将直接进入实时监测中心。"),
        card_frame);
    card_subtitle->setObjectName(QStringLiteral("pageSubtitle"));
    card_subtitle->setWordWrap(true);

    auto* form_frame = new QFrame(card_frame);
    form_frame->setProperty("card", true);
    auto* form_layout = new QFormLayout(form_frame);
    form_layout->setContentsMargins(18, 18, 18, 18);
    form_layout->setSpacing(14);

    host_edit_ = new QLineEdit(QStringLiteral("127.0.0.1"), form_frame);
    port_spin_box_ = new QSpinBox(form_frame);
    port_spin_box_->setRange(1, 65535);
    port_spin_box_->setValue(9200);
    username_edit_ = new QLineEdit(QStringLiteral("admin"), form_frame);
    password_edit_ = new QLineEdit(QStringLiteral("123456"), form_frame);
    password_edit_->setEchoMode(QLineEdit::Password);
    status_value_label_ = new QLabel(QStringLiteral("未连接"), form_frame);
    status_value_label_->setProperty("statusText", true);

    form_layout->addRow(QStringLiteral("业务服务地址"), host_edit_);
    form_layout->addRow(QStringLiteral("业务服务端口"), port_spin_box_);
    form_layout->addRow(QStringLiteral("用户名"), username_edit_);
    form_layout->addRow(QStringLiteral("密码"), password_edit_);
    form_layout->addRow(QStringLiteral("当前状态"), status_value_label_);

    auto* helper_label = new QLabel(
        QStringLiteral("默认管理员账号：admin / 123456。若已部署业务服务，请确保端口与服务端配置一致。"),
        card_frame);
    helper_label->setObjectName(QStringLiteral("tipLabel"));
    helper_label->setWordWrap(true);

    login_button_ = new QPushButton(QStringLiteral("进入监护系统"), card_frame);
    login_button_->setMinimumHeight(44);
    login_button_->setProperty("buttonRole", QStringLiteral("primary"));

    auto* log_title = new QLabel(QStringLiteral("连接日志"), card_frame);
    log_title->setObjectName(QStringLiteral("sectionTitle"));
    log_edit_ = new QPlainTextEdit(card_frame);
    log_edit_->setReadOnly(true);
    log_edit_->setMaximumBlockCount(200);
    log_edit_->setMinimumHeight(140);

    card_layout->addWidget(card_title);
    card_layout->addWidget(card_subtitle);
    card_layout->addWidget(form_frame);
    card_layout->addWidget(helper_label);
    card_layout->addWidget(login_button_);
    card_layout->addWidget(log_title);
    card_layout->addWidget(log_edit_, 1);

    root_layout->addWidget(hero_frame, 5);
    root_layout->addWidget(card_frame, 4);

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
    } else if (is_connecting) {
        status_value_label_->setText(QStringLiteral("连接中"));
    } else {
        status_value_label_->setText(QStringLiteral("未连接"));
    }
    status_value_label_->setProperty("connectionState", ConnectionStateName(state));
    RefreshWidgetStyle(status_value_label_);
}
