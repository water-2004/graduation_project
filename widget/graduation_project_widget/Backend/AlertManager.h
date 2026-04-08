#ifndef GP_QML_ALERT_MANAGER_H_
#define GP_QML_ALERT_MANAGER_H_

#include <QObject>
#include <QVariantMap>

#include "../Models/AlertModel.h"

class AlertManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(AlertModel* model READ model CONSTANT)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QVariantMap selectedAlert READ selectedAlert NOTIFY selectedAlertChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY statsChanged)
    Q_PROPERTY(int unconfirmedCount READ unconfirmedCount NOTIFY statsChanged)
    Q_PROPERTY(int criticalCount READ criticalCount NOTIFY statsChanged)

public:
    explicit AlertManager(QObject* parent = nullptr);

    AlertModel* model();
    int selectedIndex() const;
    QVariantMap selectedAlert() const;
    int totalCount() const;
    int unconfirmedCount() const;
    int criticalCount() const;

    void setAlerts(const QVector<AlertInfo>& alerts);
    void setSelectedIndex(int index);
    AlertInfo selectedAlertInfo() const;

signals:
    void selectedIndexChanged();
    void selectedAlertChanged();
    void statsChanged();

private:
    static QVariantMap ToVariantMap(const AlertInfo& alert);

    AlertModel model_;
    int selected_index_ = -1;
};

#endif  // GP_QML_ALERT_MANAGER_H_
