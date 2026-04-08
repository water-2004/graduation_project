#ifndef GP_QML_PATIENT_MODEL_H_
#define GP_QML_PATIENT_MODEL_H_

#include <QAbstractListModel>
#include <QVector>

#include "../global.h"

class PatientModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    enum Roles {
        PatientIdRole = Qt::UserRole + 1,
        NameRole,
        GenderRole,
        AgeRole,
        PhoneRole,
        RemarkRole,
        SummaryRole,
    };
    Q_ENUM(Roles)

    explicit PatientModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const;
    void setFilterText(const QString& text);
    void setPatients(const QVector<PatientInfo>& patients);

    int totalCount() const;
    PatientInfo patientAt(int row) const;

signals:
    void filterTextChanged();
    void totalCountChanged();

private:
    void RebuildVisibleRows();
    bool MatchesFilter(const PatientInfo& patient) const;

    QVector<PatientInfo> all_patients_;
    QVector<int> visible_rows_;
    QString filter_text_;
};

#endif  // GP_QML_PATIENT_MODEL_H_
