#include "AlertModel.h"

AlertModel::AlertModel(QObject* parent)
    : QAbstractListModel(parent) {
}

int AlertModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return visible_rows_.size();
}

QVariant AlertModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= visible_rows_.size()) {
        return QVariant();
    }

    const AlertInfo& alert = all_alerts_.at(visible_rows_.at(index.row()));
    switch (role) {
        case AlertIdRole:
            return alert.alert_id;
        case PatientIdRole:
            return alert.patient_id;
        case CreatedAtRole:
            return alert.created_at;
        case AlertLevelRole:
            return alert.alert_level;
        case PredLabelRole:
            return alert.pred_label;
        case ConfidenceRole:
            return alert.confidence;
        case SourceRole:
            return alert.source;
        case SampleNameRole:
            return alert.sample_name;
        case StatusRole:
            return alert.status;
        case ConfirmedAtRole:
            return alert.confirmed_at;
        case ConfirmedByRole:
            return alert.confirmed_by;
        case SummaryRole:
            return QStringLiteral("%1 · %2 · %3")
                .arg(alert.created_at, alert.patient_id, alert.pred_label);
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> AlertModel::roleNames() const {
    return {
        {AlertIdRole, "alertId"},
        {PatientIdRole, "patientId"},
        {CreatedAtRole, "createdAt"},
        {AlertLevelRole, "alertLevel"},
        {PredLabelRole, "predLabel"},
        {ConfidenceRole, "confidence"},
        {SourceRole, "source"},
        {SampleNameRole, "sampleName"},
        {StatusRole, "status"},
        {ConfirmedAtRole, "confirmedAt"},
        {ConfirmedByRole, "confirmedBy"},
        {SummaryRole, "summary"},
    };
}

QString AlertModel::filterText() const { return filter_text_; }
QString AlertModel::levelFilter() const { return level_filter_; }
QString AlertModel::statusFilter() const { return status_filter_; }

void AlertModel::setFilterText(const QString& text) {
    const QString trimmed = text.trimmed();
    if (filter_text_ == trimmed) {
        return;
    }
    filter_text_ = trimmed;
    emit filterChanged();
    RebuildVisibleRows();
}

void AlertModel::setLevelFilter(const QString& text) {
    if (level_filter_ == text) {
        return;
    }
    level_filter_ = text;
    emit filterChanged();
    RebuildVisibleRows();
}

void AlertModel::setStatusFilter(const QString& text) {
    if (status_filter_ == text) {
        return;
    }
    status_filter_ = text;
    emit filterChanged();
    RebuildVisibleRows();
}

void AlertModel::setAlerts(const QVector<AlertInfo>& alerts) {
    all_alerts_ = alerts;
    emit totalsChanged();
    RebuildVisibleRows();
}

int AlertModel::totalCount() const { return all_alerts_.size(); }

int AlertModel::unconfirmedCount() const {
    int count = 0;
    for (const AlertInfo& alert : all_alerts_) {
        if (alert.status != QStringLiteral("confirmed")) {
            ++count;
        }
    }
    return count;
}

int AlertModel::criticalCount() const {
    int count = 0;
    for (const AlertInfo& alert : all_alerts_) {
        if (alert.alert_level == QStringLiteral("critical")) {
            ++count;
        }
    }
    return count;
}

AlertInfo AlertModel::alertAt(int row) const {
    if (row < 0 || row >= visible_rows_.size()) {
        return AlertInfo();
    }
    return all_alerts_.at(visible_rows_.at(row));
}

void AlertModel::RebuildVisibleRows() {
    beginResetModel();
    visible_rows_.clear();
    for (int i = 0; i < all_alerts_.size(); ++i) {
        if (MatchesFilter(all_alerts_.at(i))) {
            visible_rows_.push_back(i);
        }
    }
    endResetModel();
}

bool AlertModel::MatchesFilter(const AlertInfo& alert) const {
    if (level_filter_ != QStringLiteral("all") && alert.alert_level != level_filter_) {
        return false;
    }
    if (status_filter_ != QStringLiteral("all") && alert.status != status_filter_) {
        return false;
    }
    if (filter_text_.isEmpty()) {
        return true;
    }
    return alert.patient_id.contains(filter_text_, Qt::CaseInsensitive) ||
           alert.pred_label.contains(filter_text_, Qt::CaseInsensitive) ||
           alert.sample_name.contains(filter_text_, Qt::CaseInsensitive) ||
           alert.alert_level.contains(filter_text_, Qt::CaseInsensitive);
}
