#include "patient_page.h"

#include "csv_exporter.h"
#include "pagination_widget.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
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
        bg = QColor(253, 236, 236);  // #fdecec
    } else if (alert_level == QStringLiteral("warning")) {
        bg = QColor(255, 244, 229);  // #fff4e5
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

PatientPage::PatientPage(QWidget* parent)
    : QWidget(parent) {
    BuildUi();
    UpdateSelectedPatientLabel();
    ClearBusinessTables();
}

void PatientPage::slot_on_user_ready(const LoginUserInfo& user_info) {
    current_user_ = user_info;
    current_user_label_->setText(
        QStringLiteral("当前登录用户：%1（%2）").arg(user_info.display_name, user_info.role));
    AppendLog(QStringLiteral("登录会话已建立，准备加载病人列表。"));
    emit sig_load_patients();
}

void PatientPage::slot_on_patient_list_ready(const QVector<PatientInfo>& patients) {
    patients_ = patients;
    ReloadPatientTable();
    AppendLog(QStringLiteral("已同步病人列表，共 %1 人。").arg(patients_.size()));

    if (patients_.isEmpty()) {
        ClearBusinessTables();
        ClearEditor();
        SetEditorMode(false, QString());
    }
}

void PatientPage::slot_on_patient_detail_ready(const PatientInfo& patient_info) {
    FillEditorFromPatient(patient_info);
    SetEditorMode(true, patient_info.patient_id);
    AppendLog(QStringLiteral("已载入病人详情：%1 - %2。")
                  .arg(patient_info.patient_id, patient_info.name));
}

void PatientPage::slot_on_patient_operation_success(const QString& message) {
    AppendLog(QStringLiteral("业务操作成功：%1").arg(message));
    ClearEditor();
    SetEditorMode(false, QString());
    emit sig_load_patients();
}

void PatientPage::slot_on_monitor_records_ready(const QVector<MonitorRecordInfo>& records) {
    monitor_records_ = records;
    ReloadMonitorRecordTable();
    AppendLog(QStringLiteral("已加载监测记录 %1 条。").arg(records.size()));
}

void PatientPage::slot_on_alerts_ready(const QVector<AlertInfo>& alerts) {
    alerts_ = alerts;
    ReloadAlertTable();
    AppendLog(QStringLiteral("已加载报警记录 %1 条。").arg(alerts.size()));
}

void PatientPage::slot_on_alert_confirmed(const QString& message) {
    AppendLog(QStringLiteral("报警确认成功：%1").arg(message));
    LoadSelectedBusinessDetails();
}

void PatientPage::slot_on_business_error(const QString& error_text) {
    AppendLog(QStringLiteral("业务操作失败：%1").arg(error_text));
    if (isVisible()) {
        QMessageBox::warning(this, QStringLiteral("业务操作失败"), error_text);
    }
}

void PatientPage::slot_on_connection_closed() {
    AppendLog(QStringLiteral("业务服务端连接已断开，请返回登录页重新建立会话。"));
}

void PatientPage::slot_append_log(const QString& text) {
    AppendLog(text);
}

void PatientPage::OnRefreshClicked() {
    emit sig_load_patients();
}

void PatientPage::OnAddClicked() {
    PatientInfo patient_info;
    QString error_text;
    if (!BuildPatientFromEditor(&patient_info, &error_text)) {
        QMessageBox::warning(this, QStringLiteral("新增失败"), error_text);
        return;
    }

    emit sig_add_patient(patient_info);
}

void PatientPage::OnUpdateClicked() {
    if (!is_edit_mode_ || editing_patient_id_.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("更新失败"), QStringLiteral("请先在表格中选择一个病人。"));
        return;
    }

    PatientInfo patient_info;
    QString error_text;
    if (!BuildPatientFromEditor(&patient_info, &error_text)) {
        QMessageBox::warning(this, QStringLiteral("更新失败"), error_text);
        return;
    }

    // 病人编号是主键，编辑模式下保持不变，只更新其他字段。
    patient_info.patient_id = editing_patient_id_;
    emit sig_update_patient(patient_info);
}

void PatientPage::OnClearEditorClicked() {
    patient_table_->clearSelection();
    ClearEditor();
    SetEditorMode(false, QString());
}

void PatientPage::OnDeleteClicked() {
    PatientInfo patient_info;
    if (!GetSelectedPatient(&patient_info)) {
        QMessageBox::warning(this, QStringLiteral("删除失败"), QStringLiteral("请先在表格中选择一个病人。"));
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("确认删除"),
        QStringLiteral("确认删除病人 %1（%2）吗？\n关联的监测记录和报警记录也会一并清理。")
            .arg(patient_info.patient_id, patient_info.name));
    if (answer != QMessageBox::Yes) {
        return;
    }

    emit sig_delete_patient(patient_info.patient_id);
}

void PatientPage::OnConfirmAlertClicked() {
    AlertInfo alert_info;
    if (!GetSelectedAlert(&alert_info)) {
        QMessageBox::warning(this, QStringLiteral("确认失败"), QStringLiteral("请先在报警记录表中选择一条报警。"));
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

void PatientPage::OnOpenAlertCenterClicked() {
    emit sig_open_alert_center();
}

void PatientPage::OnEnterMonitorClicked() {
    PatientInfo patient_info;
    if (!GetSelectedPatient(&patient_info)) {
        QMessageBox::warning(this, QStringLiteral("无法进入监测"), QStringLiteral("请先选择一个病人。"));
        return;
    }

    emit sig_enter_monitor(patient_info);
}

void PatientPage::OnExportRecordsClicked() {
    CsvExporter::ExportTableToFile(monitor_record_table_, QStringLiteral("监测记录.csv"), this);
}

void PatientPage::OnExportAlertsClicked() {
    CsvExporter::ExportTableToFile(alert_table_, QStringLiteral("报警记录.csv"), this);
}

void PatientPage::OnPatientTableSelectionChanged() {
    PatientInfo patient_info;
    if (GetSelectedPatient(&patient_info)) {
        FillEditorFromPatient(patient_info);
        SetEditorMode(true, patient_info.patient_id);
    } else {
        ClearEditor();
        SetEditorMode(false, QString());
    }

    UpdateSelectedPatientLabel();
    LoadSelectedBusinessDetails();
}

void PatientPage::BuildUi() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(24, 20, 24, 20);
    root_layout->setSpacing(16);

    auto* title_label = new QLabel(QStringLiteral("病人管理"), this);
    title_label->setObjectName(QStringLiteral("pageTitle"));

    auto* subtitle_label = new QLabel(
        QStringLiteral("先维护病人基础信息，再查看该病人的监测记录和报警记录，最后进入实时监测页面。"),
        this);
    subtitle_label->setWordWrap(true);
    subtitle_label->setObjectName(QStringLiteral("pageSubtitle"));

    auto* user_group = new QGroupBox(QStringLiteral("当前会话"), this);
    auto* user_layout = new QVBoxLayout(user_group);
    current_user_label_ = new QLabel(QStringLiteral("当前登录用户：未登录"), user_group);
    selected_patient_label_ = new QLabel(QStringLiteral("当前选中病人：未选择"), user_group);
    user_layout->addWidget(current_user_label_);
    user_layout->addWidget(selected_patient_label_);

    auto* table_group = new QGroupBox(QStringLiteral("病人列表"), this);
    auto* table_layout = new QVBoxLayout(table_group);
    patient_table_ = new QTableWidget(0, 6, table_group);
    patient_table_->setHorizontalHeaderLabels(
        {QStringLiteral("编号"),
         QStringLiteral("姓名"),
         QStringLiteral("性别"),
         QStringLiteral("年龄"),
         QStringLiteral("电话"),
         QStringLiteral("备注")});
    patient_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    patient_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    patient_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    patient_table_->setAlternatingRowColors(true);
    patient_table_->horizontalHeader()->setStretchLastSection(true);
    patient_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    patient_table_->verticalHeader()->setVisible(false);

    auto* table_button_layout = new QHBoxLayout();
    refresh_button_ = new QPushButton(QStringLiteral("刷新列表"), table_group);
    delete_button_ = new QPushButton(QStringLiteral("删除病人"), table_group);
    open_alert_center_button_ = new QPushButton(QStringLiteral("报警中心"), table_group);
    enter_monitor_button_ = new QPushButton(QStringLiteral("进入监测"), table_group);
    table_button_layout->addWidget(refresh_button_);
    table_button_layout->addWidget(delete_button_);
    table_button_layout->addWidget(open_alert_center_button_);
    table_button_layout->addStretch(1);
    table_button_layout->addWidget(enter_monitor_button_);

    table_layout->addWidget(patient_table_);
    table_layout->addLayout(table_button_layout);

    auto* detail_layout = new QHBoxLayout();
    auto* monitor_group = new QGroupBox(QStringLiteral("监测记录"), this);
    auto* monitor_layout = new QVBoxLayout(monitor_group);
    monitor_record_table_ = new QTableWidget(0, 8, monitor_group);
    monitor_record_table_->setHorizontalHeaderLabels(
        {QStringLiteral("时间"),
         QStringLiteral("来源"),
         QStringLiteral("样本"),
         QStringLiteral("真实标签"),
         QStringLiteral("预测标签"),
         QStringLiteral("置信度"),
         QStringLiteral("告警等级"),
         QStringLiteral("耗时(ms)")});
    monitor_record_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    monitor_record_table_->setSelectionMode(QAbstractItemView::NoSelection);
    monitor_record_table_->setAlternatingRowColors(true);
    monitor_record_table_->horizontalHeader()->setStretchLastSection(true);
    monitor_record_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    monitor_record_table_->verticalHeader()->setVisible(false);
    auto* export_records_button = new QPushButton(QStringLiteral("导出 CSV"), monitor_group);
    auto* monitor_button_layout = new QHBoxLayout();
    monitor_button_layout->addStretch(1);
    monitor_button_layout->addWidget(export_records_button);
    record_pagination_ = new PaginationWidget(monitor_group);
    monitor_layout->addWidget(monitor_record_table_);
    monitor_layout->addWidget(record_pagination_);
    monitor_layout->addLayout(monitor_button_layout);

    auto* alert_group = new QGroupBox(QStringLiteral("报警记录"), this);
    auto* alert_layout = new QVBoxLayout(alert_group);
    alert_table_ = new QTableWidget(0, 9, alert_group);
    alert_table_->setHorizontalHeaderLabels(
        {QStringLiteral("时间"),
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
    confirm_alert_button_ = new QPushButton(QStringLiteral("确认报警"), alert_group);
    auto* export_alerts_button = new QPushButton(QStringLiteral("导出 CSV"), alert_group);
    auto* alert_button_layout = new QHBoxLayout();
    alert_button_layout->addStretch(1);
    alert_button_layout->addWidget(export_alerts_button);
    alert_button_layout->addWidget(confirm_alert_button_);
    alert_pagination_ = new PaginationWidget(alert_group);
    alert_layout->addWidget(alert_table_);
    alert_layout->addWidget(alert_pagination_);
    alert_layout->addLayout(alert_button_layout);

    detail_layout->addWidget(monitor_group, 2);
    detail_layout->addWidget(alert_group, 1);

    auto* editor_group = new QGroupBox(QStringLiteral("病人表单"), this);
    auto* editor_layout = new QFormLayout(editor_group);
    editor_layout->setSpacing(12);
    patient_id_edit_ = new QLineEdit(editor_group);
    patient_name_edit_ = new QLineEdit(editor_group);
    gender_combo_box_ = new QComboBox(editor_group);
    gender_combo_box_->addItems({QStringLiteral("男"), QStringLiteral("女")});
    age_spin_box_ = new QSpinBox(editor_group);
    age_spin_box_->setRange(0, 120);
    age_spin_box_->setValue(30);
    phone_edit_ = new QLineEdit(editor_group);
    remark_edit_ = new QLineEdit(editor_group);
    add_button_ = new QPushButton(QStringLiteral("新增病人"), editor_group);
    update_button_ = new QPushButton(QStringLiteral("更新病人"), editor_group);
    clear_editor_button_ = new QPushButton(QStringLiteral("清空表单"), editor_group);

    auto* editor_button_layout = new QHBoxLayout();
    editor_button_layout->addWidget(add_button_);
    editor_button_layout->addWidget(update_button_);
    editor_button_layout->addWidget(clear_editor_button_);
    editor_button_layout->addStretch(1);

    editor_layout->addRow(QStringLiteral("病人编号"), patient_id_edit_);
    editor_layout->addRow(QStringLiteral("姓名"), patient_name_edit_);
    editor_layout->addRow(QStringLiteral("性别"), gender_combo_box_);
    editor_layout->addRow(QStringLiteral("年龄"), age_spin_box_);
    editor_layout->addRow(QStringLiteral("联系电话"), phone_edit_);
    editor_layout->addRow(QStringLiteral("备注"), remark_edit_);
    editor_layout->addRow(QString(), editor_button_layout);

    auto* log_group = new QGroupBox(QStringLiteral("业务日志"), this);
    auto* log_layout = new QVBoxLayout(log_group);
    log_edit_ = new QPlainTextEdit(log_group);
    log_edit_->setReadOnly(true);
    log_edit_->setMaximumBlockCount(300);
    log_layout->addWidget(log_edit_);

    root_layout->addWidget(title_label);
    root_layout->addWidget(subtitle_label);
    root_layout->addWidget(user_group);
    root_layout->addWidget(table_group, 1);
    root_layout->addLayout(detail_layout, 1);
    root_layout->addWidget(editor_group);
    root_layout->addWidget(log_group, 1);

    connect(refresh_button_, &QPushButton::clicked, this, &PatientPage::OnRefreshClicked);
    connect(add_button_, &QPushButton::clicked, this, &PatientPage::OnAddClicked);
    connect(update_button_, &QPushButton::clicked, this, &PatientPage::OnUpdateClicked);
    connect(clear_editor_button_, &QPushButton::clicked, this, &PatientPage::OnClearEditorClicked);
    connect(delete_button_, &QPushButton::clicked, this, &PatientPage::OnDeleteClicked);
    connect(confirm_alert_button_, &QPushButton::clicked, this, &PatientPage::OnConfirmAlertClicked);
    connect(open_alert_center_button_, &QPushButton::clicked, this, &PatientPage::OnOpenAlertCenterClicked);
    connect(enter_monitor_button_, &QPushButton::clicked, this, &PatientPage::OnEnterMonitorClicked);
    connect(export_records_button, &QPushButton::clicked, this, &PatientPage::OnExportRecordsClicked);
    connect(export_alerts_button, &QPushButton::clicked, this, &PatientPage::OnExportAlertsClicked);
    connect(record_pagination_, &PaginationWidget::sig_page_changed,
            this, [this](int, int, int) { ReloadMonitorRecordTable(); });
    connect(alert_pagination_, &PaginationWidget::sig_page_changed,
            this, [this](int, int, int) { ReloadAlertTable(); });
    connect(patient_table_, &QTableWidget::itemSelectionChanged,
            this, &PatientPage::OnPatientTableSelectionChanged);

    SetEditorMode(false, QString());
}

void PatientPage::ReloadPatientTable() {
    patient_table_->setUpdatesEnabled(false);
    patient_table_->setRowCount(patients_.size());

    for (int row = 0; row < patients_.size(); ++row) {
        const PatientInfo& patient_info = patients_.at(row);
        patient_table_->setItem(row, 0, new QTableWidgetItem(patient_info.patient_id));
        patient_table_->setItem(row, 1, new QTableWidgetItem(patient_info.name));
        patient_table_->setItem(row, 2, new QTableWidgetItem(patient_info.gender));
        patient_table_->setItem(row, 3, new QTableWidgetItem(QString::number(patient_info.age)));
        patient_table_->setItem(row, 4, new QTableWidgetItem(patient_info.phone));
        patient_table_->setItem(row, 5, new QTableWidgetItem(patient_info.remark));
    }
    patient_table_->setUpdatesEnabled(true);

    if (!patients_.isEmpty()) {
        patient_table_->selectRow(0);
    }
    UpdateSelectedPatientLabel();
}

void PatientPage::ReloadMonitorRecordTable() {
    record_pagination_->SetTotalItems(monitor_records_.size());
    const int start = record_pagination_->offset();
    const int end = qMin(start + record_pagination_->pageSize(), monitor_records_.size());
    const int page_count = end - start;

    monitor_record_table_->setUpdatesEnabled(false);
    monitor_record_table_->setRowCount(page_count);
    for (int i = 0; i < page_count; ++i) {
        const MonitorRecordInfo& record = monitor_records_.at(start + i);
        monitor_record_table_->setItem(i, 0, new QTableWidgetItem(record.recorded_at));
        monitor_record_table_->setItem(i, 1, new QTableWidgetItem(record.source));
        monitor_record_table_->setItem(i, 2, new QTableWidgetItem(record.sample_name));
        monitor_record_table_->setItem(i, 3, new QTableWidgetItem(record.true_label));
        monitor_record_table_->setItem(i, 4, new QTableWidgetItem(record.pred_label));
        monitor_record_table_->setItem(i, 5, new QTableWidgetItem(record.confidence));
        monitor_record_table_->setItem(i, 6, new QTableWidgetItem(record.alert_level));
        monitor_record_table_->setItem(i, 7, new QTableWidgetItem(record.latency_ms));
        ApplyAlertRowColor(monitor_record_table_, i, record.alert_level);
    }
    monitor_record_table_->setUpdatesEnabled(true);
}

void PatientPage::ReloadAlertTable() {
    alert_pagination_->SetTotalItems(alerts_.size());
    const int start = alert_pagination_->offset();
    const int end = qMin(start + alert_pagination_->pageSize(), alerts_.size());
    const int page_count = end - start;

    alert_table_->setUpdatesEnabled(false);
    alert_table_->setRowCount(page_count);
    for (int i = 0; i < page_count; ++i) {
        const AlertInfo& alert = alerts_.at(start + i);
        alert_table_->setItem(i, 0, new QTableWidgetItem(alert.created_at));
        alert_table_->setItem(i, 1, new QTableWidgetItem(alert.alert_level));
        alert_table_->setItem(i, 2, new QTableWidgetItem(alert.pred_label));
        alert_table_->setItem(i, 3, new QTableWidgetItem(alert.confidence));
        alert_table_->setItem(i, 4, new QTableWidgetItem(alert.source));
        alert_table_->setItem(i, 5, new QTableWidgetItem(alert.sample_name));
        alert_table_->setItem(i, 6, new QTableWidgetItem(alert.status));
        alert_table_->setItem(i, 7, new QTableWidgetItem(alert.confirmed_at));
        alert_table_->setItem(i, 8, new QTableWidgetItem(alert.confirmed_by));
        ApplyAlertRowColor(alert_table_, i, alert.alert_level);
    }
    alert_table_->setUpdatesEnabled(true);

    confirm_alert_button_->setEnabled(!alerts_.isEmpty());
}

void PatientPage::LoadSelectedBusinessDetails() {
    PatientInfo patient_info;
    if (!GetSelectedPatient(&patient_info)) {
        ClearBusinessTables();
        return;
    }

    emit sig_load_monitor_records(patient_info.patient_id);
    emit sig_load_alerts(patient_info.patient_id);
}

void PatientPage::ClearBusinessTables() {
    monitor_records_.clear();
    alerts_.clear();
    if (monitor_record_table_ != nullptr) {
        monitor_record_table_->setRowCount(0);
    }
    if (alert_table_ != nullptr) {
        alert_table_->setRowCount(0);
    }
    if (confirm_alert_button_ != nullptr) {
        confirm_alert_button_->setEnabled(false);
    }
}

void PatientPage::AppendLog(const QString& text) {
    log_edit_->appendPlainText(QStringLiteral("[%1] %2").arg(CurrentTimeText(), text));
}

void PatientPage::ClearEditor() {
    patient_id_edit_->clear();
    patient_name_edit_->clear();
    gender_combo_box_->setCurrentIndex(0);
    age_spin_box_->setValue(30);
    phone_edit_->clear();
    remark_edit_->clear();
}

void PatientPage::FillEditorFromPatient(const PatientInfo& patient_info) {
    patient_id_edit_->setText(patient_info.patient_id);
    patient_name_edit_->setText(patient_info.name);

    const int gender_index = gender_combo_box_->findText(patient_info.gender);
    if (gender_index >= 0) {
        gender_combo_box_->setCurrentIndex(gender_index);
    }
    age_spin_box_->setValue(patient_info.age);
    phone_edit_->setText(patient_info.phone);
    remark_edit_->setText(patient_info.remark);
}

void PatientPage::SetEditorMode(bool is_edit_mode, const QString& patient_id) {
    is_edit_mode_ = is_edit_mode;
    editing_patient_id_ = patient_id.trimmed();

    const bool editing_existing = is_edit_mode_ && !editing_patient_id_.isEmpty();
    patient_id_edit_->setReadOnly(editing_existing);
    add_button_->setEnabled(!editing_existing);
    update_button_->setEnabled(editing_existing);
}

void PatientPage::UpdateSelectedPatientLabel() {
    PatientInfo patient_info;
    if (!GetSelectedPatient(&patient_info)) {
        selected_patient_label_->setText(QStringLiteral("当前选中病人：未选择"));
        return;
    }

    selected_patient_label_->setText(
        QStringLiteral("当前选中病人：%1 - %2，%3 岁，%4")
            .arg(patient_info.patient_id, patient_info.name)
            .arg(patient_info.age)
            .arg(patient_info.gender));
}

bool PatientPage::BuildPatientFromEditor(PatientInfo* patient_info, QString* error_text) const {
    if (patient_info == nullptr || error_text == nullptr) {
        return false;
    }

    patient_info->patient_id = patient_id_edit_->text().trimmed();
    patient_info->name = patient_name_edit_->text().trimmed();
    patient_info->gender = gender_combo_box_->currentText();
    patient_info->age = age_spin_box_->value();
    patient_info->phone = phone_edit_->text().trimmed();
    patient_info->remark = remark_edit_->text().trimmed();

    if (patient_info->patient_id.isEmpty()) {
        *error_text = QStringLiteral("病人编号不能为空。");
        return false;
    }
    if (patient_info->name.isEmpty()) {
        *error_text = QStringLiteral("病人姓名不能为空。");
        return false;
    }
    return true;
}

bool PatientPage::GetSelectedPatient(PatientInfo* patient_info) const {
    if (patient_info == nullptr) {
        return false;
    }

    const QList<QTableWidgetSelectionRange> ranges = patient_table_->selectedRanges();
    if (ranges.isEmpty()) {
        return false;
    }

    const int row = ranges.constFirst().topRow();
    if (row < 0 || row >= patients_.size()) {
        return false;
    }

    *patient_info = patients_.at(row);
    return true;
}

bool PatientPage::GetSelectedAlert(AlertInfo* alert_info) const {
    if (alert_info == nullptr) {
        return false;
    }

    const QList<QTableWidgetSelectionRange> ranges = alert_table_->selectedRanges();
    if (ranges.isEmpty()) {
        return false;
    }

    const int row = ranges.constFirst().topRow();
    const int actual_index = alert_pagination_->offset() + row;
    if (actual_index < 0 || actual_index >= alerts_.size()) {
        return false;
    }

    *alert_info = alerts_.at(actual_index);
    return true;
}

