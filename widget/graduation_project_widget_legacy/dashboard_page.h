#ifndef DASHBOARD_PAGE_H
#define DASHBOARD_PAGE_H

#include <QWidget>

#include "global.h"

class QLabel;
class QTableWidget;
class QVBoxLayout;

class DashboardPage : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(QWidget* parent = nullptr);

public slots:
    void slot_on_user_ready(const LoginUserInfo& user_info);
    void slot_on_patient_list_ready(const QVector<PatientInfo>& patients);
    void slot_on_all_monitor_records_ready(const QVector<MonitorRecordInfo>& records);
    void slot_on_all_alerts_ready(const QVector<AlertInfo>& alerts);
    void slot_append_log(const QString& text);
    void slot_on_business_error(const QString& error_text);
    void slot_on_connection_closed();
    void slot_request_refresh();

signals:
    void sig_load_patients();
    void sig_load_all_monitor_records();
    void sig_load_all_alerts();

private:
    void BuildUi();
    void BuildModelMetricsSection(QVBoxLayout* parent_layout);
    void UpdateSummaryCards();

    QLabel* patient_count_label_ = nullptr;
    QLabel* record_count_label_ = nullptr;
    QLabel* alert_count_label_ = nullptr;
    QLabel* unconfirmed_count_label_ = nullptr;
    QTableWidget* recent_records_table_ = nullptr;
    QTableWidget* recent_alerts_table_ = nullptr;

    int patient_count_ = 0;
    int record_count_ = 0;
    int alert_count_ = 0;
    int unconfirmed_count_ = 0;

    QVector<MonitorRecordInfo> all_records_;
    QVector<AlertInfo> all_alerts_;
};

#endif // DASHBOARD_PAGE_H

