#include "pagination_widget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include <algorithm>

PaginationWidget::PaginationWidget(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("paginationWidget"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(6);

    info_label_ = new QLabel(this);
    first_button_ = new QPushButton(QStringLiteral("首页"), this);
    prev_button_ = new QPushButton(QStringLiteral("上一页"), this);
    next_button_ = new QPushButton(QStringLiteral("下一页"), this);
    last_button_ = new QPushButton(QStringLiteral("末页"), this);

    first_button_->setProperty("buttonRole", QStringLiteral("secondary"));
    prev_button_->setProperty("buttonRole", QStringLiteral("secondary"));
    next_button_->setProperty("buttonRole", QStringLiteral("secondary"));
    last_button_->setProperty("buttonRole", QStringLiteral("secondary"));

    first_button_->setFixedWidth(56);
    prev_button_->setFixedWidth(68);
    next_button_->setFixedWidth(68);
    last_button_->setFixedWidth(56);

    auto* size_label = new QLabel(QStringLiteral("每页"), this);
    page_size_spin_ = new QSpinBox(this);
    page_size_spin_->setRange(5, 100);
    page_size_spin_->setValue(20);
    page_size_spin_->setSuffix(QStringLiteral(" 条"));

    layout->addWidget(info_label_);
    layout->addStretch();
    layout->addWidget(size_label);
    layout->addWidget(page_size_spin_);
    layout->addWidget(first_button_);
    layout->addWidget(prev_button_);
    layout->addWidget(next_button_);
    layout->addWidget(last_button_);

    connect(first_button_, &QPushButton::clicked, this, &PaginationWidget::OnFirstClicked);
    connect(prev_button_, &QPushButton::clicked, this, &PaginationWidget::OnPrevClicked);
    connect(next_button_, &QPushButton::clicked, this, &PaginationWidget::OnNextClicked);
    connect(last_button_, &QPushButton::clicked, this, &PaginationWidget::OnLastClicked);
    connect(page_size_spin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PaginationWidget::OnPageSizeChanged);

    UpdateUi();
}

void PaginationWidget::SetTotalItems(int total) {
    total_items_ = std::max(0, total);
    if (current_page_ > totalPages()) {
        current_page_ = std::max(1, totalPages());
    }
    UpdateUi();
}

int PaginationWidget::pageSize() const {
    return page_size_spin_->value();
}

int PaginationWidget::currentPage() const {
    return current_page_;
}

int PaginationWidget::totalPages() const {
    if (total_items_ <= 0) {
        return 1;
    }
    return (total_items_ + pageSize() - 1) / pageSize();
}

int PaginationWidget::offset() const {
    return (current_page_ - 1) * pageSize();
}

void PaginationWidget::OnPrevClicked() {
    if (current_page_ > 1) {
        --current_page_;
        UpdateUi();
        EmitPageChanged();
    }
}

void PaginationWidget::OnNextClicked() {
    if (current_page_ < totalPages()) {
        ++current_page_;
        UpdateUi();
        EmitPageChanged();
    }
}

void PaginationWidget::OnFirstClicked() {
    if (current_page_ != 1) {
        current_page_ = 1;
        UpdateUi();
        EmitPageChanged();
    }
}

void PaginationWidget::OnLastClicked() {
    const int last = totalPages();
    if (current_page_ != last) {
        current_page_ = last;
        UpdateUi();
        EmitPageChanged();
    }
}

void PaginationWidget::OnPageSizeChanged(int) {
    current_page_ = 1;
    UpdateUi();
    EmitPageChanged();
}

void PaginationWidget::UpdateUi() {
    const int pages = totalPages();
    info_label_->setText(
        QStringLiteral("共 %1 条，第 %2/%3 页").arg(total_items_).arg(current_page_).arg(pages));

    first_button_->setEnabled(current_page_ > 1);
    prev_button_->setEnabled(current_page_ > 1);
    next_button_->setEnabled(current_page_ < pages);
    last_button_->setEnabled(current_page_ < pages);
}

void PaginationWidget::EmitPageChanged() {
    emit sig_page_changed(current_page_, offset(), pageSize());
}
