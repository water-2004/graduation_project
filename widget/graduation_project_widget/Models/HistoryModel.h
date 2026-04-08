#ifndef GP_QML_HISTORY_MODEL_H_
#define GP_QML_HISTORY_MODEL_H_

#include <QAbstractListModel>
#include <QVariantList>
#include <QVector>

struct MonitorHistoryItem {
    QString time;
    QString predLabel;
    QString confidence;
    QString alertLevel;
    QString latencyMs;
    QString source;
    QVariantList wavePoints;
};

class HistoryModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TimeRole = Qt::UserRole + 1,
        PredLabelRole,
        ConfidenceRole,
        AlertLevelRole,
        LatencyMsRole,
        SourceRole,
        WavePointsRole,
    };
    Q_ENUM(Roles)

    explicit HistoryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void prependItem(const MonitorHistoryItem& item, int max_count = 20);
    MonitorHistoryItem itemAt(int row) const;
    void clear();

private:
    QVector<MonitorHistoryItem> items_;
};

#endif  // GP_QML_HISTORY_MODEL_H_
