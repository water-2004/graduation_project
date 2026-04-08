#ifndef GP_QML_ALERT_MODEL_H_
#define GP_QML_ALERT_MODEL_H_

#include <QAbstractListModel>
#include <QVector>

#include "../global.h"

class AlertModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterChanged)
    Q_PROPERTY(QString levelFilter READ levelFilter WRITE setLevelFilter NOTIFY filterChanged)
    Q_PROPERTY(QString statusFilter READ statusFilter WRITE setStatusFilter NOTIFY filterChanged)

public:
    enum Roles {
        AlertIdRole = Qt::UserRole + 1,
        PatientIdRole,
        CreatedAtRole,
        AlertLevelRole,
        PredLabelRole,
        ConfidenceRole,
        SourceRole,
        SampleNameRole,
        StatusRole,
        ConfirmedAtRole,
        ConfirmedByRole,
        SummaryRole,
    };
    Q_ENUM(Roles)

    explicit AlertModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const;
    QString levelFilter() const;
    QString statusFilter() const;
    void setFilterText(const QString& text);
    void setLevelFilter(const QString& text);
    void setStatusFilter(const QString& text);

    void setAlerts(const QVector<AlertInfo>& alerts);
    int totalCount() const;
    int unconfirmedCount() const;
    int criticalCount() const;
    AlertInfo alertAt(int row) const;

signals:
    void filterChanged();
    void totalsChanged();

private:
    void RebuildVisibleRows();
    bool MatchesFilter(const AlertInfo& alert) const;

    QVector<AlertInfo> all_alerts_;
    QVector<int> visible_rows_;
    QString filter_text_;
    QString level_filter_ = QStringLiteral("all");
    QString status_filter_ = QStringLiteral("all");
};

#endif  // GP_QML_ALERT_MODEL_H_
