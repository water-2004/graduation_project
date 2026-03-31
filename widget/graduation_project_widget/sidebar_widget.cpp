#include "sidebar_widget.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SidebarWidget::SidebarWidget(QWidget* parent)
    : QWidget(parent) {
    setFixedWidth(180);
    setObjectName(QStringLiteral("sidebarWidget"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 16, 8, 16);
    root->setSpacing(4);

    auto* title = new QLabel(QStringLiteral("心电异常检测"), this);
    title->setObjectName(QStringLiteral("sidebarTitle"));
    title->setAlignment(Qt::AlignCenter);
    title->setWordWrap(true);
    root->addWidget(title);
    root->addSpacing(12);

    button_layout_ = new QVBoxLayout();
    button_layout_->setSpacing(4);
    root->addLayout(button_layout_);
    root->addStretch();
}

int SidebarWidget::AddItem(const QString& text) {
    const int index = buttons_.size();
    auto* button = new QPushButton(text, this);
    button->setCheckable(true);
    button->setObjectName(QStringLiteral("sidebarButton"));
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(40);

    connect(button, &QPushButton::clicked, this, [this, index]() {
        OnButtonClicked(index);
    });

    button_layout_->addWidget(button);
    buttons_.push_back(button);
    return index;
}

void SidebarWidget::SetCurrentIndex(int index) {
    if (index < 0 || index >= buttons_.size()) {
        return;
    }

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
        buttons_[index]->setChecked(true);
        return;
    }

    SetCurrentIndex(index);
    emit sig_index_changed(index);
}
