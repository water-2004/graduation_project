#include "AlertManager.h"

AlertManager::AlertManager(QObject* parent)
    : QObject(parent) {
}

AlertModel* AlertManager::model() { return &model_; }
int AlertManager::selectedIndex() const { return selected_index_; }
QVariantMap AlertManager::selectedAlert() const { return ToVariantMap(selectedAlertInfo()); }
int AlertManager::totalCount() const { return model_.totalCount(); }
int AlertManager::unconfirmedCount() const { return model_.unconfirmedCount(); }
int AlertManager::criticalCount() const { return model_.criticalCount(); }

void AlertManager::setAlerts(const QVector<AlertInfo>& alerts) {
    model_.setAlerts(alerts);
    if (selected_index_ >= model_.rowCount()) {
        selected_index_ = model_.rowCount() > 0 ? 0 : -1;
    } else if (selected_index_ < 0 && model_.rowCount() > 0) {
        selected_index_ = 0;
    }
    emit statsChanged();
    emit selectedIndexChanged();
    emit selectedAlertChanged();
}

void AlertManager::setSelectedIndex(int index) {
    if (selected_index_ == index) {
        return;
    }
    selected_index_ = index;
    emit selectedIndexChanged();
    emit selectedAlertChanged();
}

AlertInfo AlertManager::selectedAlertInfo() const {
    return model_.alertAt(selected_index_);
}

QVariantMap AlertManager::ToVariantMap(const AlertInfo& alert) {
    return {
        {QStringLiteral("alertId"), alert.alert_id},
        {QStringLiteral("patientId"), alert.patient_id},
        {QStringLiteral("createdAt"), alert.created_at},
        {QStringLiteral("alertLevel"), alert.alert_level},
        {QStringLiteral("predLabel"), alert.pred_label},
        {QStringLiteral("confidence"), alert.confidence},
        {QStringLiteral("source"), alert.source},
        {QStringLiteral("sampleName"), alert.sample_name},
        {QStringLiteral("status"), alert.status},
        {QStringLiteral("confirmedAt"), alert.confirmed_at},
        {QStringLiteral("confirmedBy"), alert.confirmed_by},
    };
}
