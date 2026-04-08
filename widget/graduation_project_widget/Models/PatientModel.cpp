#include "PatientModel.h"

PatientModel::PatientModel(QObject* parent)
    : QAbstractListModel(parent) {
}

int PatientModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return visible_rows_.size();
}

QVariant PatientModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= visible_rows_.size()) {
        return QVariant();
    }

    const PatientInfo& patient = all_patients_.at(visible_rows_.at(index.row()));
    switch (role) {
        case PatientIdRole:
            return patient.patient_id;
        case NameRole:
            return patient.name;
        case GenderRole:
            return patient.gender;
        case AgeRole:
            return patient.age;
        case PhoneRole:
            return patient.phone;
        case RemarkRole:
            return patient.remark;
        case SummaryRole:
            return QStringLiteral("%1 · %2岁 · %3")
                .arg(patient.gender)
                .arg(patient.age)
                .arg(patient.phone);
        default:
            return QVariant();
    }
}

QHash<int, QByteArray> PatientModel::roleNames() const {
    return {
        {PatientIdRole, "patientId"},
        {NameRole, "name"},
        {GenderRole, "gender"},
        {AgeRole, "age"},
        {PhoneRole, "phone"},
        {RemarkRole, "remark"},
        {SummaryRole, "summary"},
    };
}

QString PatientModel::filterText() const {
    return filter_text_;
}

void PatientModel::setFilterText(const QString& text) {
    const QString trimmed = text.trimmed();
    if (filter_text_ == trimmed) {
        return;
    }
    filter_text_ = trimmed;
    emit filterTextChanged();
    RebuildVisibleRows();
}

void PatientModel::setPatients(const QVector<PatientInfo>& patients) {
    all_patients_ = patients;
    emit totalCountChanged();
    RebuildVisibleRows();
}

int PatientModel::totalCount() const {
    return all_patients_.size();
}

PatientInfo PatientModel::patientAt(int row) const {
    if (row < 0 || row >= visible_rows_.size()) {
        return PatientInfo();
    }
    return all_patients_.at(visible_rows_.at(row));
}

void PatientModel::RebuildVisibleRows() {
    beginResetModel();
    visible_rows_.clear();
    for (int i = 0; i < all_patients_.size(); ++i) {
        if (MatchesFilter(all_patients_.at(i))) {
            visible_rows_.push_back(i);
        }
    }
    endResetModel();
}

bool PatientModel::MatchesFilter(const PatientInfo& patient) const {
    if (filter_text_.isEmpty()) {
        return true;
    }
    return patient.patient_id.contains(filter_text_, Qt::CaseInsensitive) ||
           patient.name.contains(filter_text_, Qt::CaseInsensitive) ||
           patient.gender.contains(filter_text_, Qt::CaseInsensitive) ||
           patient.phone.contains(filter_text_, Qt::CaseInsensitive) ||
           patient.remark.contains(filter_text_, Qt::CaseInsensitive);
}
