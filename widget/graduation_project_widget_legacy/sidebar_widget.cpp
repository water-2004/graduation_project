#include "sidebar_widget.h"

#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {

void RefreshWidgetStyle(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

}  // namespace

SidebarWidget::SidebarWidget(QWidget* parent)
    : QWidget(parent) {
    setFixedWidth(248);
    setObjectName(QStringLiteral("Sidebar"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 24, 20, 20);
    root->setSpacing(10);

    auto* brand_label = new QLabel(QStringLiteral("ECG 智能监护系统"), this);
    brand_label->setObjectName(QStringLiteral("sidebarTitle"));
    brand_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    brand_label->setWordWrap(true);
    root->addWidget(brand_label);

    auto* sub_title = new QLabel(QStringLiteral("Edge AI Monitoring"), this);
    sub_title->setObjectName(QStringLiteral("sidebarCaption"));
    sub_title->setWordWrap(true);
    root->addWidget(sub_title);
    root->addSpacing(12);

    button_layout_ = new QVBoxLayout();
    button_layout_->setSpacing(4);
    root->addLayout(button_layout_);
    root->addStretch();

    auto* footer_layout = new QVBoxLayout();
    footer_layout->setSpacing(8);

    about_button_ = new QPushButton(QStringLiteral("关于系统"), this);
    about_button_->setProperty("sidebarFooter", true);
    about_button_->setProperty("buttonRole", QStringLiteral("secondary"));
    about_button_->setCursor(Qt::PointingHandCursor);
    footer_layout->addWidget(about_button_);

    logout_button_ = new QPushButton(QStringLiteral("退出登录"), this);
    logout_button_->setProperty("sidebarFooter", true);
    logout_button_->setProperty("buttonRole", QStringLiteral("danger"));
    logout_button_->setCursor(Qt::PointingHandCursor);
    footer_layout->addWidget(logout_button_);

    root->addLayout(footer_layout);

    connect(about_button_, &QPushButton::clicked, this, &SidebarWidget::sig_about_clicked);
    connect(logout_button_, &QPushButton::clicked, this, &SidebarWidget::sig_logout_clicked);
}

int SidebarWidget::AddItem(const QString& text) {
    const int index = buttons_.size();
    auto* button = new QPushButton(text, this);
    button->setCheckable(true);
    button->setAutoExclusive(true);
    button->setProperty("property_nav", true);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(46);

    connect(button, &QPushButton::clicked, this, [this, index]() {
        OnButtonClicked(index);
    });

    button_layout_->addWidget(button);
    buttons_.push_back(button);
    RefreshWidgetStyle(button);
    RefreshWidgetStyle(this);
    return index;
}

void SidebarWidget::SetCurrentIndex(int index) {
    current_index_ = index;
    for (int i = 0; i < buttons_.size(); ++i) {
        buttons_[i]->setChecked(i == index);
    }
}

void SidebarWidget::SetItemEnabled(int index, bool enabled) {
    if (index >= 0 && index < buttons_.size()) {
        buttons_[index]->setEnabled(enabled);
    }
}

void SidebarWidget::OnButtonClicked(int index) {
    if (index == current_index_) {
        if (index >= 0 && index < buttons_.size()) {
            buttons_[index]->setChecked(true);
        }
        return;
    }

    SetCurrentIndex(index);
    emit sig_index_changed(index);
}
