#include "EcgDataStream.h"

EcgDataStream::EcgDataStream(QObject* parent)
    : QObject(parent) {
}

QVariantList EcgDataStream::points() const {
    return points_;
}

QString EcgDataStream::alertLevel() const {
    return alert_level_;
}

void EcgDataStream::setPoints(const QVariantList& points, const QString& alert_level) {
    points_ = points;
    alert_level_ = alert_level.isEmpty() ? QStringLiteral("normal") : alert_level;
    emit pointsChanged();
    emit alertLevelChanged();
}

void EcgDataStream::clear() {
    points_.clear();
    alert_level_ = QStringLiteral("normal");
    emit pointsChanged();
    emit alertLevelChanged();
}
