#ifndef PATIENT_PAGE_H
#define PATIENT_PAGE_H

#include <QVector>
#include <QWidget>

#include "global.h"

class QAction;
class PaginationWidget;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

// 病人管理页：负责病人列表展示、新增、修改、删除、历史记录查看，以及进入监测页面。
class PatientPage : public QWidget {
    Q_OBJECT

public:
    explicit PatientPage(QWidget* parent = nullptr);

signals:
    void sig_load_patients();
    void sig_add_patient(const PatientInfo& patient_info);
    void sig_update_patient(const PatientInfo& patient_info);
    void sig_delete_patient(const QString& patient_id);
    void sig_load_monitor_records(const QString& patient_id);
    void sig_load_alerts(const QString& patient_id);
    void sig_confirm_alert(const QString& alert_id, const QString& confirmed_by);
    void sig_open_alert_center();
    void sig_enter_monitor(const PatientInfo& patient_info);

public slots:
    void slot_on_user_ready(const LoginUserInfo& user_info);
    void slot_on_patient_list_ready(const QVector<PatientInfo>& patients);
    void slot_on_patient_detail_ready(const PatientInfo& patient_info);
    void slot_on_patient_operation_success(const QString& message);
    void slot_on_monitor_records_ready(const QVector<MonitorRecordInfo>& records);
    void slot_on_alerts_ready(const QVector<AlertInfo>& alerts);
    void slot_on_alert_confirmed(const QString& message);
    void slot_on_business_error(const QString& error_text);
    void slot_on_connection_closed();
    void slot_append_log(const QString& text);

private slots:
    void OnRefreshClicked();
    void OnAddClicked();
    void OnUpdateClicked();
    void OnClearEditorClicked();
    void OnDeleteClicked();
    void OnConfirmAlertClicked();
    void OnOpenAlertCenterClicked();
    void OnEnterMonitorClicked();
    void OnExportRecordsClicked();
    void OnExportAlertsClicked();
    void OnPatientTableSelectionChanged();

private:
    void BuildUi();
    void ReloadPatientTable();
    void ReloadMonitorRecordTable();
    void ReloadAlertTable();
    void LoadSelectedBusinessDetails();
    void ClearBusinessTables();
    void AppendLog(const QString& text);
    void ClearEditor();
    void FillEditorFromPatient(const PatientInfo& patient_info);
    void SetEditorMode(bool is_edit_mode, const QString& patient_id);
    void UpdateSelectedPatientLabel();
    bool BuildPatientFromEditor(PatientInfo* patient_info, QString* error_text) const;
    bool GetSelectedPatient(PatientInfo* patient_info) const;
    bool GetSelectedAlert(AlertInfo* alert_info) const;

    LoginUserInfo current_user_;
    QVector<PatientInfo> patients_;
    QVector<MonitorRecordInfo> monitor_records_;
    QVector<AlertInfo> alerts_;
    bool is_edit_mode_ = false;
    QString editing_patient_id_;

    QLabel* current_user_label_ = nullptr;
    QLabel* selected_patient_label_ = nullptr;
    QTableWidget* patient_table_ = nullptr;
    QTableWidget* monitor_record_table_ = nullptr;
    QTableWidget* alert_table_ = nullptr;
    QPushButton* refresh_button_ = nullptr;
    QPushButton* delete_button_ = nullptr;
    QPushButton* confirm_alert_button_ = nullptr;
    QPushButton* open_alert_center_button_ = nullptr;
    QPushButton* enter_monitor_button_ = nullptr;
    QLineEdit* patient_id_edit_ = nullptr;
    QLineEdit* patient_name_edit_ = nullptr;
    QComboBox* gender_combo_box_ = nullptr;
    QSpinBox* age_spin_box_ = nullptr;
    QLineEdit* phone_edit_ = nullptr;
    QLineEdit* remark_edit_ = nullptr;
    QPushButton* add_button_ = nullptr;
    QPushButton* update_button_ = nullptr;
    QPushButton* clear_editor_button_ = nullptr;
    PaginationWidget* record_pagination_ = nullptr;
    PaginationWidget* alert_pagination_ = nullptr;
    QPlainTextEdit* log_edit_ = nullptr;
};

#endif // PATIENT_PAGE_H

