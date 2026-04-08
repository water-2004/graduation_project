#include "HistoryModel.h"

HistoryModel::HistoryModel(QObject* parent)
    : QAbstractListModel(parent) {
}

int HistoryModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return items_.size();
}

QVariant HistoryModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size()) {
        return QVariant();
    }

    const MonitorHistoryItem& item = items_.at(index.row());
    switch (role) {
        case TimeRole: return item.time;
        case PredLabelRole: return item.predLabel;
        case ConfidenceRole: return item.confidence;
        case AlertLevelRole: return item.alertLevel;
        case LatencyMsRole: return item.latencyMs;
        case SourceRole: return item.source;
        case WavePointsRole: return item.wavePoints;
        default: return QVariant();
    }
}

QHash<int, QByteArray> HistoryModel::roleNames() const {
    return {
        {TimeRole, "time"},
        {PredLabelRole, "predLabel"},
        {ConfidenceRole, "confidence"},
        {AlertLevelRole, "alertLevel"},
        {LatencyMsRole, "latencyMs"},
        {SourceRole, "source"},
        {WavePointsRole, "wavePoints"},
    };
}

void HistoryModel::prependItem(const MonitorHistoryItem& item, int max_count) {
    beginResetModel();
    items_.prepend(item);
    while (items_.size() > max_count) {
        items_.removeLast();
    }
    endResetModel();
}

MonitorHistoryItem HistoryModel::itemAt(int row) const {
    if (row < 0 || row >= items_.size()) {
        return MonitorHistoryItem();
    }
    return items_.at(row);
}

void HistoryModel::clear() {
    beginResetModel();
    items_.clear();
    endResetModel();
}
