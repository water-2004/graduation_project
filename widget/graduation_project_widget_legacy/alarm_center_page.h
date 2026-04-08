#ifndef ALARM_CENTER_PAGE_H
#define ALARM_CENTER_PAGE_H

#include <QVector>
#include <QWidget>

#include "global.h"

class PaginationWidget;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTableWidget;

// 报警中心页：负责展示全局报警列表、筛选条件、详情查看和报警确认操作。
class AlarmCenterPage : public QWidget {
    Q_OBJECT

public:
    explicit AlarmCenterPage(QWidget* parent = nullptr);

signals:
    void sig_load_patients();
    void sig_load_all_alerts();
    void sig_confirm_alert(const QString& alert_id, const QString& confirmed_by);
    void sig_back_to_patients();

public slots:
    void slot_on_user_ready(const LoginUserInfo& user_info);
    void slot_on_patient_list_ready(const QVector<PatientInfo>& patients);
    void slot_on_all_alerts_ready(const QVector<AlertInfo>& alerts);
    void slot_on_alert_confirmed(const QString& message);
    void slot_on_business_error(const QString& error_text);
    void slot_on_connection_closed();
    void slot_append_log(const QString& text);
    void slot_request_refresh();

private slots:
    void OnRefreshClicked();
    void OnBackClicked();
    void OnConfirmAlertClicked();
    void OnViewDetailClicked();
    void OnExportClicked();
    void OnFilterChanged();
    void OnTableSelectionChanged();
    void OnTableCellDoubleClicked(int row, int column);

private:
    void BuildUi();
    void ApplyFilters();
    void ReloadAlertTable();
    void UpdateSummaryLabel();
    void UpdateConfirmButtonState();
    void AppendLog(const QString& text);
    QString FindPatientName(const QString& patient_id) const;
    const PatientInfo* FindPatient(const QString& patient_id) const;
    bool MatchesFilter(const AlertInfo& alert_info) const;
    bool GetSelectedFilteredAlert(AlertInfo* alert_info) const;

    LoginUserInfo current_user_;
    QVector<PatientInfo> patients_;
    QVector<AlertInfo> alerts_;
    QVector<int> filtered_indices_;

    QLabel* current_user_label_ = nullptr;
    QLabel* summary_label_ = nullptr;
    QLineEdit* keyword_edit_ = nullptr;
    QComboBox* level_combo_box_ = nullptr;
    QComboBox* status_combo_box_ = nullptr;
    QTableWidget* alert_table_ = nullptr;
    QPushButton* refresh_button_ = nullptr;
    QPushButton* detail_button_ = nullptr;
    QPushButton* confirm_button_ = nullptr;
    QPushButton* back_button_ = nullptr;
    PaginationWidget* pagination_ = nullptr;
    QPlainTextEdit* log_edit_ = nullptr;
};

#endif // ALARM_CENTER_PAGE_H
