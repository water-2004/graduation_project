#include "TimelineModel.h"

TimelineModel::TimelineModel(QObject* parent)
    : QAbstractListModel(parent) {
}

int TimelineModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return items_.size();
}

QVariant TimelineModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= items_.size()) {
        return QVariant();
    }

    const TimelineItem& item = items_.at(index.row());
    switch (role) {
        case TimeRole: return item.time;
        case TitleRole: return item.title;
        case DetailRole: return item.detail;
        case LevelRole: return item.level;
        default: return QVariant();
    }
}

QHash<int, QByteArray> TimelineModel::roleNames() const {
    return {
        {TimeRole, "time"},
        {TitleRole, "title"},
        {DetailRole, "detail"},
        {LevelRole, "level"},
    };
}

void TimelineModel::setItems(const QVector<TimelineItem>& items) {
    beginResetModel();
    items_ = items;
    endResetModel();
}
