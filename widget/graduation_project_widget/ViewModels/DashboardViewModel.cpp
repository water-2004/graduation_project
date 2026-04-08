#include "DashboardViewModel.h"

#include <QDate>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <algorithm>

namespace {

QVariantMap MakePoint(const QString& label, double value) {
    return {{QStringLiteral("label"), label}, {QStringLiteral("value"), value}};
}

QByteArray LoadMetricsJson() {
    const QStringList candidates = {
        QStringLiteral(":/qt/qml/graduation_project/widget/data/model_metrics.json"),
        QStringLiteral(":/data/model_metrics.json"),
    };

    for (const QString& path : candidates) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            return file.readAll();
        }
    }
    return {};
}

}  // namespace

DashboardViewModel::DashboardViewModel(QObject* parent)
    : QObject(parent)
    , timeline_model_(this) {
    loadMetrics();
}

int DashboardViewModel::patientCount() const { return patient_count_; }
int DashboardViewModel::recordCount() const { return record_count_; }
int DashboardViewModel::alertCount() const { return alert_count_; }
int DashboardViewModel::unconfirmedAlertCount() const { return unconfirmed_alert_count_; }
double DashboardViewModel::modelAccuracy() const { return model_accuracy_; }
double DashboardViewModel::macroF1() const { return macro_f1_; }
double DashboardViewModel::macroRecall() const { return macro_recall_; }
QVariantList DashboardViewModel::trendPoints() const { return trend_points_; }
QVariantList DashboardViewModel::classDistribution() const { return class_distribution_; }
QVariantList DashboardViewModel::radarMetrics() const { return radar_metrics_; }
TimelineModel* DashboardViewModel::timelineModel() { return &timeline_model_; }

void DashboardViewModel::setPatients(const QVector<PatientInfo>& patients) {
    patient_count_ = patients.size();
    emit summaryChanged();
}

void DashboardViewModel::setAllRecords(const QVector<MonitorRecordInfo>& records) {
    records_ = records;
    record_count_ = records_.size();
    RebuildCharts();
    RebuildTimeline();
    emit summaryChanged();
    emit chartsChanged();
}

void DashboardViewModel::setAllAlerts(const QVector<AlertInfo>& alerts) {
    alerts_ = alerts;
    alert_count_ = alerts_.size();
    unconfirmed_alert_count_ = 0;
    for (const AlertInfo& alert : alerts_) {
        if (alert.status != QStringLiteral("confirmed")) {
            ++unconfirmed_alert_count_;
        }
    }
    RebuildCharts();
    RebuildTimeline();
    emit summaryChanged();
    emit chartsChanged();
}

void DashboardViewModel::loadMetrics() {
    const QByteArray json_data = LoadMetricsJson();
    if (json_data.isEmpty()) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(json_data);
    if (!document.isObject()) {
        return;
    }

    const QJsonObject object = document.object();
    model_accuracy_ = object.value(QStringLiteral("test_accuracy")).toDouble();
    macro_f1_ = object.value(QStringLiteral("test_macro_f1")).toDouble();
    macro_recall_ = object.value(QStringLiteral("test_macro_recall")).toDouble();

    radar_metrics_.clear();
    radar_metrics_.push_back(MakePoint(QStringLiteral("Accuracy"), model_accuracy_));
    radar_metrics_.push_back(MakePoint(QStringLiteral("Macro-F1"), macro_f1_));
    radar_metrics_.push_back(MakePoint(QStringLiteral("Macro-Recall"), macro_recall_));

    const QJsonArray recalls = object.value(QStringLiteral("test_per_class_recall")).toArray();
    const QStringList labels = {
        QStringLiteral("N"),
        QStringLiteral("S"),
        QStringLiteral("V"),
        QStringLiteral("F"),
        QStringLiteral("Q"),
    };
    for (int i = 0; i < recalls.size() && i < labels.size(); ++i) {
        radar_metrics_.push_back(MakePoint(labels.at(i), recalls.at(i).toDouble()));
    }

    emit metricsChanged();
    emit chartsChanged();
}

void DashboardViewModel::RebuildCharts() {
    trend_points_.clear();
    class_distribution_.clear();

    QMap<QDate, int> trend_map;
    const QDate today = QDate::currentDate();
    for (int i = 6; i >= 0; --i) {
        trend_map.insert(today.addDays(-i), 0);
    }
    for (const AlertInfo& alert : alerts_) {
        const QDate date = QDate::fromString(alert.created_at.left(10), QStringLiteral("yyyy-MM-dd"));
        if (trend_map.contains(date)) {
            trend_map[date] += 1;
        }
    }
    for (auto it = trend_map.cbegin(); it != trend_map.cend(); ++it) {
        trend_points_.push_back(MakePoint(it.key().toString(QStringLiteral("MM/dd")), it.value()));
    }

    QMap<QString, int> class_counts;
    for (const MonitorRecordInfo& record : records_) {
        class_counts[record.pred_label] += 1;
    }

    QList<QPair<QString, int>> pairs;
    for (auto it = class_counts.cbegin(); it != class_counts.cend(); ++it) {
        pairs.push_back(qMakePair(it.key(), it.value()));
    }
    std::sort(pairs.begin(), pairs.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.second > rhs.second;
    });
    for (const auto& pair : pairs) {
        class_distribution_.push_back(MakePoint(pair.first, pair.second));
    }
}

void DashboardViewModel::RebuildTimeline() {
    QVector<TimelineItem> items;
    for (int i = 0; i < alerts_.size() && items.size() < 5; ++i) {
        const AlertInfo& alert = alerts_.at(i);
        TimelineItem item;
        item.time = alert.created_at;
        item.title = QStringLiteral("报警触发");
        item.detail = QStringLiteral("病人 %1 · %2 · %3")
                          .arg(alert.patient_id, alert.pred_label, alert.alert_level);
        item.level = alert.alert_level;
        items.push_back(item);
    }
    timeline_model_.setItems(items);
}
