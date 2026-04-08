#include "about_page.h"

#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {

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

AboutPage::AboutPage(QWidget* parent) : QWidget(parent) {
    BuildUi();
}

void AboutPage::RefreshStatus() {
    UpdateStatusIndicator(business_dot_, business_text_, business_state_);
    UpdateStatusIndicator(inference_dot_, inference_text_, inference_state_);
}

void AboutPage::SetBusinessState(ClientState state) {
    business_state_ = state;
    UpdateStatusIndicator(business_dot_, business_text_, state);
}

void AboutPage::SetInferenceState(ClientState state) {
    inference_state_ = state;
    UpdateStatusIndicator(inference_dot_, inference_text_, state);
}

void AboutPage::SetCurrentUser(const QString& username) {
    current_username_ = username;
}

void AboutPage::slot_on_password_changed(const QString& message) {
    QMessageBox::information(this, QStringLiteral("修改密码"), message);
}

void AboutPage::slot_on_business_error(const QString& error_text) {
    if (isVisible()) {
        QMessageBox::warning(this, QStringLiteral("操作失败"), error_text);
    }
}

void AboutPage::BuildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("关于本系统"));
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    auto* desc = new QLabel(
        QStringLiteral("心电异常检测桌面客户端\n\n"
                       "本系统基于深度学习模型，实现心电信号的实时采集、推理分析与报警管理。\n"
                       "支持多病人管理、监测记录查询、报警中心等功能。\n\n"
                       "技术栈: Qt6 + C++ 后端 (Boost.Asio) + ONNX Runtime 推理引擎"));
    desc->setWordWrap(true);
    desc->setObjectName(QStringLiteral("pageSubtitle"));
    layout->addWidget(desc);

    layout->addSpacing(16);

    auto* status_title = new QLabel(QStringLiteral("服务连接状态"));
    status_title->setObjectName(QStringLiteral("pageSubtitle"));
    layout->addWidget(status_title);

    auto make_status_row = [&](const QString& label_text, QLabel*& dot, QLabel*& text) {
        auto* row = new QHBoxLayout();
        row->setSpacing(8);

        auto* name_label = new QLabel(label_text);
        name_label->setFixedWidth(120);
        row->addWidget(name_label);

        dot = new QLabel();
        dot->setProperty("statusDot", true);
        row->addWidget(dot);

        text = new QLabel();
        text->setProperty("statusText", true);
        row->addWidget(text);
        row->addStretch();

        layout->addLayout(row);
    };

    make_status_row(QStringLiteral("业务服务端:"), business_dot_, business_text_);
    make_status_row(QStringLiteral("推理服务端:"), inference_dot_, inference_text_);

    RefreshStatus();

    layout->addSpacing(16);

    auto* pw_button = new QPushButton(QStringLiteral("修改密码"), this);
    pw_button->setFixedWidth(120);
    pw_button->setProperty("buttonRole", QStringLiteral("primary"));
    layout->addWidget(pw_button);
    connect(pw_button, &QPushButton::clicked, this, &AboutPage::OnChangePasswordClicked);

    layout->addStretch();

    auto* version_label = new QLabel(QStringLiteral("版本 1.0.0"));
    version_label->setObjectName(QStringLiteral("tipLabel"));
    layout->addWidget(version_label);
}

void AboutPage::OnChangePasswordClicked() {
    if (current_username_.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("修改密码"), QStringLiteral("请先登录"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("修改密码"));
    dialog.setMinimumWidth(350);

    auto* form = new QFormLayout(&dialog);
    auto* old_edit = new QLineEdit(&dialog);
    old_edit->setEchoMode(QLineEdit::Password);
    auto* new_edit = new QLineEdit(&dialog);
    new_edit->setEchoMode(QLineEdit::Password);
    auto* confirm_edit = new QLineEdit(&dialog);
    confirm_edit->setEchoMode(QLineEdit::Password);
    auto* submit = new QPushButton(QStringLiteral("确认修改"), &dialog);
    submit->setProperty("buttonRole", QStringLiteral("primary"));

    form->addRow(QStringLiteral("原密码"), old_edit);
    form->addRow(QStringLiteral("新密码"), new_edit);
    form->addRow(QStringLiteral("确认新密码"), confirm_edit);
    form->addRow(QString(), submit);

    connect(submit, &QPushButton::clicked, &dialog, [&]() {
        if (old_edit->text().isEmpty() || new_edit->text().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("修改密码"), QStringLiteral("密码不能为空"));
            return;
        }
        if (new_edit->text() != confirm_edit->text()) {
            QMessageBox::warning(&dialog, QStringLiteral("修改密码"), QStringLiteral("两次输入的新密码不一致"));
            return;
        }
        emit sig_change_password(current_username_, old_edit->text(), new_edit->text());
        dialog.accept();
    });

    dialog.exec();
}

void AboutPage::UpdateStatusIndicator(QLabel* dot_label, QLabel* text_label, ClientState state) {
    if (dot_label == nullptr || text_label == nullptr) {
        return;
    }

    switch (state) {
        case ClientState::Connected:
            text_label->setText(QStringLiteral("已连接"));
            break;
        case ClientState::Connecting:
            text_label->setText(QStringLiteral("连接中..."));
            break;
        case ClientState::Disconnected:
            text_label->setText(QStringLiteral("未连接"));
            break;
    }

    const QString state_name = ConnectionStateName(state);
    dot_label->setProperty("connectionState", state_name);
    text_label->setProperty("connectionState", state_name);
    RefreshWidgetStyle(dot_label);
    RefreshWidgetStyle(text_label);
}
