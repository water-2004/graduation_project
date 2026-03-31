#ifndef CSV_EXPORTER_H
#define CSV_EXPORTER_H

#include <QString>
#include <QTableWidget>

namespace CsvExporter {

bool ExportTableToFile(QTableWidget* table, const QString& default_filename, QWidget* parent);

}  // namespace CsvExporter

#endif // CSV_EXPORTER_H
