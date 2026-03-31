#include "alert_detail_dialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QString LevelBadgeStyle(const QString& alert_level) {
    if (alert_level == QStringLiteral("critical")) {
        return QStringLiteral(
            "padding:6px 14px;border-radius:14px;background:#8d1f1f;color:white;font-weight:700;");
    }
    if (alert_level == QStringLiteral("warning")) {
        return QStringLiteral(
            "padding:6px 14px;border-radius:14px;background:#b56700;color:white;font-weight:700;");
    }
    return QStringLiteral(
        "padding:6px 14px;border-radius:14px;background:#4d6a7f;color:white;font-weight:700;");
}

}  // namespace

AlertDetailDialog::AlertDetailDialog(QWidget* parent)
    : QDialog(parent) {
    BuildUi();
}

void AlertDetailDialog::SetAlertData(const AlertInfo& alert_info, const PatientInfo* patient_info) {
    setWindowTitle(QStringLiteral("报警详情 - %1").arg(DisplayText(alert_info.alert_id)));

    level_badge_label_->setText(DisplayText(alert_info.alert_level));
    level_badge_label_->setStyleSheet(LevelBadgeStyle(alert_info.alert_level));
    summary_text_edit_->setPlainText(BuildSummaryText(alert_info, patient_info));

    if (patient_info != nullptr) {
        patient_id_value_label_->setText(DisplayText(patient_info->patient_id));
        patient_name_value_label_->setText(DisplayText(patient_info->name));
        gender_value_label_->setText(DisplayText(patient_info->gender));
        age_value_label_->setText(QString::number(patient_info->age));
        phone_value_label_->setText(DisplayText(patient_info->phone));
        remark_value_label_->setText(DisplayText(patient_info->remark));
    } else {
        patient_id_value_label_->setText(DisplayText(alert_info.patient_id));
        patient_name_value_label_->setText(QStringLiteral("未找到对应病人"));
        gender_value_label_->setText(QStringLiteral("-"));
        age_value_label_->setText(QStringLiteral("-"));
        phone_value_label_->setText(QStringLiteral("-"));
        remark_value_label_->setText(QStringLiteral("-"));
    }

    alert_id_value_label_->setText(DisplayText(alert_info.alert_id));
    created_at_value_label_->setText(DisplayText(alert_info.created_at));
    alert_level_value_label_->setText(DisplayText(alert_info.alert_level));
    pred_label_value_label_->setText(DisplayText(alert_info.pred_label));
    confidence_value_label_->setText(DisplayText(alert_info.confidence));
    source_value_label_->setText(DisplayText(alert_info.source));
    sample_name_value_label_->setText(DisplayText(alert_info.sample_name));
    status_value_label_->setText(DisplayText(alert_info.status));
    confirmed_at_value_label_->setText(DisplayText(alert_info.confirmed_at));
    confirmed_by_value_label_->setText(DisplayText(alert_info.confirmed_by));
}

void AlertDetailDialog::BuildUi() {
    resize(760, 680);

    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(20, 18, 20, 18);
    root_layout->setSpacing(14);

    auto* title_layout = new QHBoxLayout();
    auto* title_label = new QLabel(QStringLiteral("报警详情"), this);
    title_label->setObjectName(QStringLiteral("pageTitle"));
    level_badge_label_ = new QLabel(QStringLiteral("-"), this);
    title_layout->addWidget(title_label);
    title_layout->addStretch(1);
    title_layout->addWidget(level_badge_label_);

    auto* summary_group = new QGroupBox(QStringLiteral("处理摘要"), this);
    auto* summary_layout = new QVBoxLayout(summary_group);
    summary_text_edit_ = new QTextEdit(summary_group);
    summary_text_edit_->setReadOnly(true);
    summary_text_edit_->setMinimumHeight(120);
    summary_layout->addWidget(summary_text_edit_);

    auto* patient_group = new QGroupBox(QStringLiteral("病人信息"), this);
    auto* patient_layout = new QFormLayout(patient_group);
    patient_id_value_label_ = CreateValueLabel(patient_group);
    patient_name_value_label_ = CreateValueLabel(patient_group);
    gender_value_label_ = CreateValueLabel(patient_group);
    age_value_label_ = CreateValueLabel(patient_group);
    phone_value_label_ = CreateValueLabel(patient_group);
    remark_value_label_ = CreateValueLabel(patient_group);
    patient_layout->addRow(QStringLiteral("病人编号"), patient_id_value_label_);
    patient_layout->addRow(QStringLiteral("病人姓名"), patient_name_value_label_);
    patient_layout->addRow(QStringLiteral("性别"), gender_value_label_);
    patient_layout->addRow(QStringLiteral("年龄"), age_value_label_);
    patient_layout->addRow(QStringLiteral("联系电话"), phone_value_label_);
    patient_layout->addRow(QStringLiteral("备注"), remark_value_label_);

    auto* alert_group = new QGroupBox(QStringLiteral("报警记录"), this);
    auto* alert_layout = new QFormLayout(alert_group);
    alert_id_value_label_ = CreateValueLabel(alert_group);
    created_at_value_label_ = CreateValueLabel(alert_group);
    alert_level_value_label_ = CreateValueLabel(alert_group);
    pred_label_value_label_ = CreateValueLabel(alert_group);
    confidence_value_label_ = CreateValueLabel(alert_group);
    source_value_label_ = CreateValueLabel(alert_group);
    sample_name_value_label_ = CreateValueLabel(alert_group);
    status_value_label_ = CreateValueLabel(alert_group);
    confirmed_at_value_label_ = CreateValueLabel(alert_group);
    confirmed_by_value_label_ = CreateValueLabel(alert_group);
    alert_layout->addRow(QStringLiteral("报警编号"), alert_id_value_label_);
    alert_layout->addRow(QStringLiteral("触发时间"), created_at_value_label_);
    alert_layout->addRow(QStringLiteral("告警级别"), alert_level_value_label_);
    alert_layout->addRow(QStringLiteral("预测标签"), pred_label_value_label_);
    alert_layout->addRow(QStringLiteral("置信度"), confidence_value_label_);
    alert_layout->addRow(QStringLiteral("数据来源"), source_value_label_);
    alert_layout->addRow(QStringLiteral("样本名称"), sample_name_value_label_);
    alert_layout->addRow(QStringLiteral("当前状态"), status_value_label_);
    alert_layout->addRow(QStringLiteral("确认时间"), confirmed_at_value_label_);
    alert_layout->addRow(QStringLiteral("确认人"), confirmed_by_value_label_);

    auto* button_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    root_layout->addLayout(title_layout);
    root_layout->addWidget(summary_group);
    root_layout->addWidget(patient_group);
    root_layout->addWidget(alert_group);
    root_layout->addWidget(button_box);
}

QLabel* AlertDetailDialog::CreateValueLabel(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setStyleSheet(QStringLiteral("padding:4px 0;color:#243746;background:transparent;border:none;"));
    return label;
}

QString AlertDetailDialog::DisplayText(const QString& text) {
    const QString trimmed = text.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("-") : trimmed;
}

QString AlertDetailDialog::BuildSummaryText(
    const AlertInfo& alert_info,
    const PatientInfo* patient_info) const {
    const QString patient_name = patient_info == nullptr
        ? QStringLiteral("未知病人")
        : DisplayText(patient_info->name);

    QString status_text;
    if (alert_info.status == QStringLiteral("confirmed")) {
        status_text = QStringLiteral("该报警已经完成确认，可继续结合监测记录进行复核。");
    } else {
        status_text = QStringLiteral("该报警当前仍处于未确认状态，需要值班人员进一步处理。");
    }

    return QStringLiteral(
               "病人 %1（%2）在 %3 触发了一条 %4 级报警。\n"
               "模型当前预测标签为 %5，置信度为 %6。\n"
               "本次数据来源为 %7，样本名称为 %8。\n"
               "%9")
        .arg(patient_name,
             DisplayText(alert_info.patient_id),
             DisplayText(alert_info.created_at),
             DisplayText(alert_info.alert_level),
             DisplayText(alert_info.pred_label),
             DisplayText(alert_info.confidence),
             DisplayText(alert_info.source),
             DisplayText(alert_info.sample_name),
             status_text);
}
