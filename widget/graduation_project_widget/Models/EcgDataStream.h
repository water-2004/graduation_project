#ifndef GP_QML_ECG_DATA_STREAM_H_
#define GP_QML_ECG_DATA_STREAM_H_

#include <QObject>
#include <QVariantList>

class EcgDataStream : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList points READ points NOTIFY pointsChanged)
    Q_PROPERTY(QString alertLevel READ alertLevel NOTIFY alertLevelChanged)

public:
    explicit EcgDataStream(QObject* parent = nullptr);

    QVariantList points() const;
    QString alertLevel() const;

    void setPoints(const QVariantList& points, const QString& alert_level);
    void clear();

signals:
    void pointsChanged();
    void alertLevelChanged();

private:
    QVariantList points_;
    QString alert_level_ = QStringLiteral("normal");
};

#endif  // GP_QML_ECG_DATA_STREAM_H_
