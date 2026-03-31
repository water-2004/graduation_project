#include "csv_exporter.h"

#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QTextStream>

namespace CsvExporter {

static QString EscapeCsvField(const QString& field) {
    if (field.contains(',') || field.contains('"') || field.contains('\n')) {
        QString escaped = field;
        escaped.replace('"', QStringLiteral("\"\""));
        return '"' + escaped + '"';
    }
    return field;
}

bool ExportTableToFile(QTableWidget* table, const QString& default_filename, QWidget* parent) {
    if (table == nullptr || table->rowCount() == 0) {
        QMessageBox::information(parent, QStringLiteral("导出"),
                                 QStringLiteral("没有可导出的数据"));
        return false;
    }

    const QString file_path = QFileDialog::getSaveFileName(
        parent, QStringLiteral("导出 CSV"), default_filename,
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*)"));
    if (file_path.isEmpty()) {
        return false;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(parent, QStringLiteral("导出失败"),
                             QStringLiteral("无法打开文件: %1").arg(file.errorString()));
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    // BOM for Excel compatibility
    stream << QChar(0xFEFF);

    // Header
    const int col_count = table->columnCount();
    for (int c = 0; c < col_count; ++c) {
        if (c > 0) {
            stream << ',';
        }
        auto* header_item = table->horizontalHeaderItem(c);
        stream << EscapeCsvField(header_item != nullptr ? header_item->text() : QString());
    }
    stream << '\n';

    // Rows
    const int row_count = table->rowCount();
    for (int r = 0; r < row_count; ++r) {
        for (int c = 0; c < col_count; ++c) {
            if (c > 0) {
                stream << ',';
            }
            auto* item = table->item(r, c);
            stream << EscapeCsvField(item != nullptr ? item->text() : QString());
        }
        stream << '\n';
    }

    file.close();
    QMessageBox::information(parent, QStringLiteral("导出成功"),
                             QStringLiteral("已导出 %1 条记录到:\n%2").arg(row_count).arg(file_path));
    return true;
}

}  // namespace CsvExporter
