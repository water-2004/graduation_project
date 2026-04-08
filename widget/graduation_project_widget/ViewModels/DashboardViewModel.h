#ifndef GP_QML_DASHBOARD_VIEW_MODEL_H_
#define GP_QML_DASHBOARD_VIEW_MODEL_H_

#include <QObject>
#include <QVariantList>
#include <QVector>

#include "../Models/TimelineModel.h"
#include "../global.h"

class DashboardViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int patientCount READ patientCount NOTIFY summaryChanged)
    Q_PROPERTY(int recordCount READ recordCount NOTIFY summaryChanged)
    Q_PROPERTY(int alertCount READ alertCount NOTIFY summaryChanged)
    Q_PROPERTY(int unconfirmedAlertCount READ unconfirmedAlertCount NOTIFY summaryChanged)
    Q_PROPERTY(double modelAccuracy READ modelAccuracy NOTIFY metricsChanged)
    Q_PROPERTY(double macroF1 READ macroF1 NOTIFY metricsChanged)
    Q_PROPERTY(double macroRecall READ macroRecall NOTIFY metricsChanged)
    Q_PROPERTY(QVariantList trendPoints READ trendPoints NOTIFY chartsChanged)
    Q_PROPERTY(QVariantList classDistribution READ classDistribution NOTIFY chartsChanged)
    Q_PROPERTY(QVariantList radarMetrics READ radarMetrics NOTIFY chartsChanged)
    Q_PROPERTY(TimelineModel* timelineModel READ timelineModel CONSTANT)

public:
    explicit DashboardViewModel(QObject* parent = nullptr);

    int patientCount() const;
    int recordCount() const;
    int alertCount() const;
    int unconfirmedAlertCount() const;
    double modelAccuracy() const;
    double macroF1() const;
    double macroRecall() const;
    QVariantList trendPoints() const;
    QVariantList classDistribution() const;
    QVariantList radarMetrics() const;
    TimelineModel* timelineModel();

    void setPatients(const QVector<PatientInfo>& patients);
    void setAllRecords(const QVector<MonitorRecordInfo>& records);
    void setAllAlerts(const QVector<AlertInfo>& alerts);
    void loadMetrics();

signals:
    void summaryChanged();
    void metricsChanged();
    void chartsChanged();

private:
    void RebuildCharts();
    void RebuildTimeline();

    int patient_count_ = 0;
    int record_count_ = 0;
    int alert_count_ = 0;
    int unconfirmed_alert_count_ = 0;
    double model_accuracy_ = 0.0;
    double macro_f1_ = 0.0;
    double macro_recall_ = 0.0;
    QVariantList trend_points_;
    QVariantList class_distribution_;
    QVariantList radar_metrics_;
    QVector<MonitorRecordInfo> records_;
    QVector<AlertInfo> alerts_;
    TimelineModel timeline_model_;
};

#endif  // GP_QML_DASHBOARD_VIEW_MODEL_H_
