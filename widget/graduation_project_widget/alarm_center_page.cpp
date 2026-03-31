#include "alarm_center_page.h"

#include "alert_detail_dialog.h"
#include "csv_exporter.h"
#include "pagination_widget.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

QString CurrentTimeText() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

void ApplyAlertRowColor(QTableWidget* table, int row, const QString& alert_level) {
    QColor bg;
    if (alert_level == QStringLiteral("critical")) {
        bg = QColor(253, 236, 236);
    } else if (alert_level == QStringLiteral("warning")) {
        bg = QColor(255, 244, 229);
    } else {
        return;
    }
    for (int c = 0; c < table->columnCount(); ++c) {
        auto* item = table->item(row, c);
        if (item != nullptr) {
            item->setBackground(bg);
        }
    }
}

}  // namespace

AlarmCenterPage::AlarmCenterPage(QWidget* parent)
    : QWidget(parent) {
    BuildUi();
    UpdateSummaryLabel();
    UpdateConfirmButtonState();
}

void AlarmCenterPage::slot_on_user_ready(const LoginUserInfo& user_info) {
    current_user_ = user_info;
    current_user_label_->setText(
        QStringLiteral("当前登录用户：%1（%2）").arg(user_info.display_name, user_info.role));
}

void AlarmCenterPage::slot_on_patient_list_ready(const QVector<PatientInfo>& patients) {
    patients_ = patients;
    ApplyFilters();
}

void AlarmCenterPage::slot_on_all_alerts_ready(const QVector<AlertInfo>& alerts) {
    alerts_ = alerts;
    ApplyFilters();
    AppendLog(QStringLiteral("已同步全局报警列表，共 %1 条。").arg(alerts_.size()));
}

void AlarmCenterPage::slot_on_alert_confirmed(const QString& message) {
    AppendLog(QStringLiteral("报警确认成功：%1").arg(message));
    emit sig_load_all_alerts();
}

void AlarmCenterPage::slot_on_business_error(const QString& error_text) {
    AppendLog(QStringLiteral("业务操作失败：%1").arg(error_text));
    if (isVisible()) {
        QMessageBox::warning(this, QStringLiteral("业务操作失败"), error_text);
    }
}

void AlarmCenterPage::slot_on_connection_closed() {
    AppendLog(QStringLiteral("业务服务端连接已断开，请返回登录页重新建立会话。"));
}

void AlarmCenterPage::slot_append_log(const QString& text) {
    AppendLog(text);
}

void AlarmCenterPage::slot_request_refresh() {
    AppendLog(QStringLiteral("准备刷新全局报警列表。"));
    emit sig_load_patients();
    emit sig_load_all_alerts();
}

void AlarmCenterPage::OnRefreshClicked() {
    slot_request_refresh();
}

void AlarmCenterPage::OnBackClicked() {
    emit sig_back_to_patients();
}

void AlarmCenterPage::OnConfirmAlertClicked() {
    AlertInfo alert_info;
    if (!GetSelectedFilteredAlert(&alert_info)) {
        QMessageBox::warning(this, QStringLiteral("确认失败"),
                             QStringLiteral("请先在报警中心表格中选择一条报警。"));
        return;
    }

    if (alert_info.status == QStringLiteral("confirmed")) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("该报警已确认。"));
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("确认报警"),
        QStringLiteral("确认将报警 %1 标记为已确认吗？").arg(alert_info.alert_id));
    if (answer != QMessageBox::Yes) {
        return;
    }

    emit sig_confirm_alert(alert_info.alert_id, current_user_.username);
}

void AlarmCenterPage::OnViewDetailClicked() {
    AlertInfo alert_info;
    if (!GetSelectedFilteredAlert(&alert_info)) {
        QMessageBox::information(this, QStringLiteral("查看详情"),
                                 QStringLiteral("请先在报警中心表格中选择一条报警。"));
        return;
    }

    AlertDetailDialog dialog(this);
    dialog.SetAlertData(alert_info, FindPatient(alert_info.patient_id));
    AppendLog(QStringLiteral("打开报警详情：%1").arg(alert_info.alert_id));
    dialog.exec();
}

void AlarmCenterPage::OnExportClicked() {
    CsvExporter::ExportTableToFile(alert_table_, QStringLiteral("全局报警记录.csv"), this);
}

void AlarmCenterPage::OnFilterChanged() {
    ApplyFilters();
}

void AlarmCenterPage::OnTableSelectionChanged() {
    UpdateConfirmButtonState();
}

void AlarmCenterPage::OnTableCellDoubleClicked(int row, int column) {
    Q_UNUSED(row);
    Q_UNUSED(column);
    OnViewDetailClicked();
}

void AlarmCenterPage::BuildUi() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(24, 20, 24, 20);
    root_layout->setSpacing(16);

    auto* title_label = new QLabel(QStringLiteral("报警中心"), this);
    title_label->setObjectName(QStringLiteral("pageTitle"));

    auto* subtitle_label = new QLabel(
        QStringLiteral("集中查看系统中的全部异常报警，并按病人、级别、状态进行筛选、详情查看和确认处理。"),
        this);
    subtitle_label->setWordWrap(true);
    subtitle_label->setObjectName(QStringLiteral("pageSubtitle"));

    auto* session_group = new QGroupBox(QStringLiteral("当前会话"), this);
    auto* session_layout = new QVBoxLayout(session_group);
    current_user_label_ = new QLabel(QStringLiteral("当前登录用户：未登录"), session_group);
    summary_label_ = new QLabel(QStringLiteral("报警统计：总数 0，未确认 0，critical 0"), session_group);
    session_layout->addWidget(current_user_label_);
    session_layout->addWidget(summary_label_);

    auto* filter_group = new QGroupBox(QStringLiteral("筛选条件"), this);
    auto* filter_layout = new QGridLayout(filter_group);
    keyword_edit_ = new QLineEdit(filter_group);
    keyword_edit_->setPlaceholderText(QStringLiteral("输入病人编号、病人姓名、预测标签或样本名"));
    level_combo_box_ = new QComboBox(filter_group);
    level_combo_box_->addItems(
        {QStringLiteral("全部级别"), QStringLiteral("critical"), QStringLiteral("warning")});
    status_combo_box_ = new QComboBox(filter_group);
    status_combo_box_->addItems(
        {QStringLiteral("全部状态"), QStringLiteral("new"), QStringLiteral("confirmed")});
    refresh_button_ = new QPushButton(QStringLiteral("刷新报警"), filter_group);
    back_button_ = new QPushButton(QStringLiteral("返回病人管理"), filter_group);

    filter_layout->addWidget(new QLabel(QStringLiteral("关键词"), filter_group), 0, 0);
    filter_layout->addWidget(keyword_edit_, 0, 1, 1, 3);
    filter_layout->addWidget(new QLabel(QStringLiteral("级别"), filter_group), 1, 0);
    filter_layout->addWidget(level_combo_box_, 1, 1);
    filter_layout->addWidget(new QLabel(QStringLiteral("状态"), filter_group), 1, 2);
    filter_layout->addWidget(status_combo_box_, 1, 3);
    filter_layout->addWidget(refresh_button_, 2, 2);
    filter_layout->addWidget(back_button_, 2, 3);

    auto* table_group = new QGroupBox(QStringLiteral("全局报警列表"), this);
    auto* table_layout = new QVBoxLayout(table_group);
    alert_table_ = new QTableWidget(0, 11, table_group);
    alert_table_->setHorizontalHeaderLabels(
        {QStringLiteral("时间"),
         QStringLiteral("病人编号"),
         QStringLiteral("病人姓名"),
         QStringLiteral("级别"),
         QStringLiteral("预测标签"),
         QStringLiteral("置信度"),
         QStringLiteral("来源"),
         QStringLiteral("样本"),
         QStringLiteral("状态"),
         QStringLiteral("确认时间"),
         QStringLiteral("确认人")});
    alert_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    alert_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    alert_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    alert_table_->setAlternatingRowColors(true);
    alert_table_->horizontalHeader()->setStretchLastSection(true);
    alert_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    alert_table_->verticalHeader()->setVisible(false);
    detail_button_ = new QPushButton(QStringLiteral("查看报警详情"), table_group);
    confirm_button_ = new QPushButton(QStringLiteral("确认选中报警"), table_group);
    auto* export_button = new QPushButton(QStringLiteral("导出 CSV"), table_group);
    auto* table_button_layout = new QHBoxLayout();
    table_button_layout->addStretch(1);
    table_button_layout->addWidget(export_button);
    table_button_layout->addWidget(detail_button_);
    table_button_layout->addWidget(confirm_button_);
    pagination_ = new PaginationWidget(table_group);
    table_layout->addWidget(alert_table_);
    table_layout->addWidget(pagination_);
    table_layout->addLayout(table_button_layout);

    auto* log_group = new QGroupBox(QStringLiteral("报警中心日志"), this);
    auto* log_layout = new QVBoxLayout(log_group);
    log_edit_ = new QPlainTextEdit(log_group);
    log_edit_->setReadOnly(true);
    log_edit_->setMaximumBlockCount(300);
    log_layout->addWidget(log_edit_);

    root_layout->addWidget(title_label);
    root_layout->addWidget(subtitle_label);
    root_layout->addWidget(session_group);
    root_layout->addWidget(filter_group);
    root_layout->addWidget(table_group, 1);
    root_layout->addWidget(log_group, 1);

    connect(refresh_button_, &QPushButton::clicked, this, &AlarmCenterPage::OnRefreshClicked);
    connect(back_button_, &QPushButton::clicked, this, &AlarmCenterPage::OnBackClicked);
    connect(confirm_button_, &QPushButton::clicked, this, &AlarmCenterPage::OnConfirmAlertClicked);
    connect(detail_button_, &QPushButton::clicked, this, &AlarmCenterPage::OnViewDetailClicked);
    connect(export_button, &QPushButton::clicked, this, &AlarmCenterPage::OnExportClicked);
    connect(keyword_edit_, &QLineEdit::textChanged, this, &AlarmCenterPage::OnFilterChanged);
    connect(level_combo_box_, &QComboBox::currentTextChanged, this, &AlarmCenterPage::OnFilterChanged);
    connect(status_combo_box_, &QComboBox::currentTextChanged, this, &AlarmCenterPage::OnFilterChanged);
    connect(alert_table_, &QTableWidget::itemSelectionChanged,
            this, &AlarmCenterPage::OnTableSelectionChanged);
    connect(alert_table_, &QTableWidget::cellDoubleClicked,
            this, &AlarmCenterPage::OnTableCellDoubleClicked);
    connect(pagination_, &PaginationWidget::sig_page_changed,
            this, [this](int, int, int) { ReloadAlertTable(); });
}

void AlarmCenterPage::ApplyFilters() {
    filtered_indices_.clear();
    for (int i = 0; i < alerts_.size(); ++i) {
        if (MatchesFilter(alerts_.at(i))) {
            filtered_indices_.push_back(i);
        }
    }

    ReloadAlertTable();
    UpdateSummaryLabel();
    UpdateConfirmButtonState();
}

void AlarmCenterPage::ReloadAlertTable() {
    pagination_->SetTotalItems(filtered_indices_.size());
    const int start = pagination_->offset();
    const int end = qMin(start + pagination_->pageSize(), filtered_indices_.size());
    const int page_count = end - start;

    alert_table_->setUpdatesEnabled(false);
    alert_table_->clearContents();
    alert_table_->setRowCount(page_count);
    for (int i = 0; i < page_count; ++i) {
        const AlertInfo& alert = alerts_.at(filtered_indices_.at(start + i));
        alert_table_->setItem(i, 0, new QTableWidgetItem(alert.created_at));
        alert_table_->setItem(i, 1, new QTableWidgetItem(alert.patient_id));
        alert_table_->setItem(i, 2, new QTableWidgetItem(FindPatientName(alert.patient_id)));
        alert_table_->setItem(i, 3, new QTableWidgetItem(alert.alert_level));
        alert_table_->setItem(i, 4, new QTableWidgetItem(alert.pred_label));
        alert_table_->setItem(i, 5, new QTableWidgetItem(alert.confidence));
        alert_table_->setItem(i, 6, new QTableWidgetItem(alert.source));
        alert_table_->setItem(i, 7, new QTableWidgetItem(alert.sample_name));
        alert_table_->setItem(i, 8, new QTableWidgetItem(alert.status));
        alert_table_->setItem(i, 9, new QTableWidgetItem(alert.confirmed_at));
        alert_table_->setItem(i, 10, new QTableWidgetItem(alert.confirmed_by));
        ApplyAlertRowColor(alert_table_, i, alert.alert_level);
    }
    alert_table_->setUpdatesEnabled(true);
}

void AlarmCenterPage::UpdateSummaryLabel() {
    int new_count = 0;
    int critical_count = 0;
    for (const AlertInfo& alert : alerts_) {
        if (alert.status == QStringLiteral("new")) {
            ++new_count;
        }
        if (alert.alert_level == QStringLiteral("critical")) {
            ++critical_count;
        }
    }

    summary_label_->setText(
        QStringLiteral("报警统计：总数 %1，未确认 %2，critical %3，当前筛选结果 %4")
            .arg(alerts_.size())
            .arg(new_count)
            .arg(critical_count)
            .arg(filtered_indices_.size()));
}

void AlarmCenterPage::UpdateConfirmButtonState() {
    AlertInfo alert_info;
    const bool has_selection = GetSelectedFilteredAlert(&alert_info);
    detail_button_->setEnabled(has_selection);
    confirm_button_->setEnabled(has_selection && alert_info.status != QStringLiteral("confirmed"));
}

void AlarmCenterPage::AppendLog(const QString& text) {
    log_edit_->appendPlainText(QStringLiteral("[%1] %2").arg(CurrentTimeText(), text));
}

QString AlarmCenterPage::FindPatientName(const QString& patient_id) const {
    for (const PatientInfo& patient : patients_) {
        if (patient.patient_id == patient_id) {
            return patient.name;
        }
    }
    return QStringLiteral("-");
}

const PatientInfo* AlarmCenterPage::FindPatient(const QString& patient_id) const {
    for (const PatientInfo& patient : patients_) {
        if (patient.patient_id == patient_id) {
            return &patient;
        }
    }
    return nullptr;
}

bool AlarmCenterPage::MatchesFilter(const AlertInfo& alert_info) const {
    const QString level_filter = level_combo_box_->currentText();
    if (level_filter != QStringLiteral("全部级别") && alert_info.alert_level != level_filter) {
        return false;
    }

    const QString status_filter = status_combo_box_->currentText();
    if (status_filter != QStringLiteral("全部状态") && alert_info.status != status_filter) {
        return false;
    }

    const QString keyword = keyword_edit_->text().trimmed();
    if (keyword.isEmpty()) {
        return true;
    }

    const QString patient_name = FindPatientName(alert_info.patient_id);
    return alert_info.patient_id.contains(keyword, Qt::CaseInsensitive) ||
           patient_name.contains(keyword, Qt::CaseInsensitive) ||
           alert_info.pred_label.contains(keyword, Qt::CaseInsensitive) ||
           alert_info.sample_name.contains(keyword, Qt::CaseInsensitive);
}

bool AlarmCenterPage::GetSelectedFilteredAlert(AlertInfo* alert_info) const {
    if (alert_info == nullptr) {
        return false;
    }

    const QList<QTableWidgetSelectionRange> ranges = alert_table_->selectedRanges();
    if (ranges.isEmpty()) {
        return false;
    }

    const int row = ranges.constFirst().topRow();
    const int actual_filtered_index = pagination_->offset() + row;
    if (actual_filtered_index < 0 || actual_filtered_index >= filtered_indices_.size()) {
        return false;
    }

    const int alert_index = filtered_indices_.at(actual_filtered_index);
    if (alert_index < 0 || alert_index >= alerts_.size()) {
        return false;
    }

    *alert_info = alerts_.at(alert_index);
    return true;
}
