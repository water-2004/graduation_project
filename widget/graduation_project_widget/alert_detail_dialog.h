#ifndef ALERT_DETAIL_DIALOG_H
#define ALERT_DETAIL_DIALOG_H

#include <QDialog>

#include "global.h"

class QLabel;
class QTextEdit;

// 报警详情弹窗：负责展示选中报警的完整业务信息，便于老师演示和用户排查。
class AlertDetailDialog : public QDialog {
    Q_OBJECT

public:
    explicit AlertDetailDialog(QWidget* parent = nullptr);

    void SetAlertData(const AlertInfo& alert_info, const PatientInfo* patient_info);

private:
    void BuildUi();
    static QLabel* CreateValueLabel(QWidget* parent);
    static QString DisplayText(const QString& text);
    QString BuildSummaryText(const AlertInfo& alert_info, const PatientInfo* patient_info) const;

    QLabel* level_badge_label_ = nullptr;
    QTextEdit* summary_text_edit_ = nullptr;

    QLabel* patient_id_value_label_ = nullptr;
    QLabel* patient_name_value_label_ = nullptr;
    QLabel* gender_value_label_ = nullptr;
    QLabel* age_value_label_ = nullptr;
    QLabel* phone_value_label_ = nullptr;
    QLabel* remark_value_label_ = nullptr;

    QLabel* alert_id_value_label_ = nullptr;
    QLabel* created_at_value_label_ = nullptr;
    QLabel* alert_level_value_label_ = nullptr;
    QLabel* pred_label_value_label_ = nullptr;
    QLabel* confidence_value_label_ = nullptr;
    QLabel* source_value_label_ = nullptr;
    QLabel* sample_name_value_label_ = nullptr;
    QLabel* status_value_label_ = nullptr;
    QLabel* confirmed_at_value_label_ = nullptr;
    QLabel* confirmed_by_value_label_ = nullptr;
};

#endif // ALERT_DETAIL_DIALOG_H
