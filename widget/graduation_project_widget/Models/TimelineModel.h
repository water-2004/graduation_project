#ifndef GP_QML_TIMELINE_MODEL_H_
#define GP_QML_TIMELINE_MODEL_H_

#include <QAbstractListModel>
#include <QVector>

struct TimelineItem {
    QString time;
    QString title;
    QString detail;
    QString level;
};

class TimelineModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TimeRole = Qt::UserRole + 1,
        TitleRole,
        DetailRole,
        LevelRole,
    };
    Q_ENUM(Roles)

    explicit TimelineModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(const QVector<TimelineItem>& items);

private:
    QVector<TimelineItem> items_;
};

#endif  // GP_QML_TIMELINE_MODEL_H_
