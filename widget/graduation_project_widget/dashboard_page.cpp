#include "dashboard_page.h"

#include <QColor>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

void ApplyAlertRowColor(QTableWidget* table, int row, const QString& alert_level) {
    QColor bg;
    if (alert_level == QStringLiteral("critical")) {
        bg = QColor(253, 236, 236);
    } else if (alert_level == QStringLiteral("warning")) {
        bg = QColor(255, 244, 229);
    } else {
        return;
    }
    for (int c = 0; c < table->columnCount(); ++c) {
        auto* item = table->item(row, c);
        if (item != nullptr) {
            item->setBackground(bg);
        }
    }
}

}  // namespace

DashboardPage::DashboardPage(QWidget* parent) : QWidget(parent) {
    BuildUi();
}

void DashboardPage::slot_on_user_ready(const LoginUserInfo&) {
    slot_request_refresh();
}

void DashboardPage::slot_on_patient_list_ready(const QVector<PatientInfo>& patients) {
    patient_count_ = patients.size();
    UpdateSummaryCards();
}

void DashboardPage::slot_on_all_monitor_records_ready(const QVector<MonitorRecordInfo>& records) {
    all_records_ = records;
    record_count_ = records.size();
    UpdateSummaryCards();

    recent_records_table_->setRowCount(0);
    const int show_count = qMin(records.size(), 10);
    recent_records_table_->setRowCount(show_count);
    for (int i = 0; i < show_count; ++i) {
        const auto& r = records[i];
        recent_records_table_->setItem(i, 0, new QTableWidgetItem(r.recorded_at));
        recent_records_table_->setItem(i, 1, new QTableWidgetItem(r.patient_id));
        recent_records_table_->setItem(i, 2, new QTableWidgetItem(r.pred_label));
        recent_records_table_->setItem(i, 3, new QTableWidgetItem(r.confidence));
        recent_records_table_->setItem(i, 4, new QTableWidgetItem(r.alert_level));
        recent_records_table_->setItem(i, 5, new QTableWidgetItem(r.source));
        ApplyAlertRowColor(recent_records_table_, i, r.alert_level);
    }
}

void DashboardPage::slot_on_all_alerts_ready(const QVector<AlertInfo>& alerts) {
    all_alerts_ = alerts;
    alert_count_ = alerts.size();
    unconfirmed_count_ = 0;
    for (const auto& a : alerts) {
        if (a.status != QStringLiteral("confirmed")) {
            ++unconfirmed_count_;
        }
    }
    UpdateSummaryCards();

    recent_alerts_table_->setRowCount(0);
    const int show_count = qMin(alerts.size(), 10);
    recent_alerts_table_->setRowCount(show_count);
    for (int i = 0; i < show_count; ++i) {
        const auto& a = alerts[i];
        recent_alerts_table_->setItem(i, 0, new QTableWidgetItem(a.created_at));
        recent_alerts_table_->setItem(i, 1, new QTableWidgetItem(a.patient_id));
        recent_alerts_table_->setItem(i, 2, new QTableWidgetItem(a.alert_level));
        recent_alerts_table_->setItem(i, 3, new QTableWidgetItem(a.pred_label));
        recent_alerts_table_->setItem(i, 4, new QTableWidgetItem(a.status));
        ApplyAlertRowColor(recent_alerts_table_, i, a.alert_level);
    }
}

void DashboardPage::slot_append_log(const QString&) {
}

void DashboardPage::slot_on_business_error(const QString&) {
}

void DashboardPage::slot_on_connection_closed() {
    patient_count_ = 0;
    record_count_ = 0;
    alert_count_ = 0;
    unconfirmed_count_ = 0;
    all_records_.clear();
    all_alerts_.clear();
    UpdateSummaryCards();
    recent_records_table_->setRowCount(0);
    recent_alerts_table_->setRowCount(0);
}

void DashboardPage::slot_request_refresh() {
    emit sig_load_patients();
    emit sig_load_all_monitor_records();
    emit sig_load_all_alerts();
}

void DashboardPage::BuildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto* title = new QLabel(QStringLiteral("系统概览"));
    title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(title);

    // Summary cards
    auto* cards_layout = new QHBoxLayout();
    cards_layout->setSpacing(16);

    auto make_card = [&](const QString& card_title, QLabel*& value_label) {
        auto* card = new QGroupBox(card_title);
        card->setObjectName(QStringLiteral("dashboardCard"));
        card->setMinimumHeight(100);
        auto* card_layout = new QVBoxLayout(card);
        value_label = new QLabel(QStringLiteral("0"));
        value_label->setObjectName(QStringLiteral("cardValue"));
        value_label->setAlignment(Qt::AlignCenter);
        QFont font = value_label->font();
        font.setPointSize(28);
        font.setBold(true);
        value_label->setFont(font);
        card_layout->addWidget(value_label);
        cards_layout->addWidget(card);
    };

    make_card(QStringLiteral("病人总数"), patient_count_label_);
    make_card(QStringLiteral("监测记录"), record_count_label_);
    make_card(QStringLiteral("报警总数"), alert_count_label_);
    make_card(QStringLiteral("待确认报警"), unconfirmed_count_label_);

    layout->addLayout(cards_layout);

    // Recent records table
    auto* records_group = new QGroupBox(QStringLiteral("最近监测记录 (最近10条)"));
    auto* records_layout = new QVBoxLayout(records_group);
    recent_records_table_ = new QTableWidget(0, 6);
    recent_records_table_->setHorizontalHeaderLabels(
        {QStringLiteral("时间"), QStringLiteral("病人编号"), QStringLiteral("预测类别"),
         QStringLiteral("置信度"), QStringLiteral("告警级别"), QStringLiteral("来源")});
    recent_records_table_->horizontalHeader()->setStretchLastSection(true);
    recent_records_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recent_records_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    recent_records_table_->verticalHeader()->setVisible(false);
    records_layout->addWidget(recent_records_table_);
    layout->addWidget(records_group);

    // Recent alerts table
    auto* alerts_group = new QGroupBox(QStringLiteral("最近报警记录 (最近10条)"));
    auto* alerts_layout = new QVBoxLayout(alerts_group);
    recent_alerts_table_ = new QTableWidget(0, 5);
    recent_alerts_table_->setHorizontalHeaderLabels(
        {QStringLiteral("时间"), QStringLiteral("病人编号"), QStringLiteral("告警级别"),
         QStringLiteral("预测类别"), QStringLiteral("状态")});
    recent_alerts_table_->horizontalHeader()->setStretchLastSection(true);
    recent_alerts_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recent_alerts_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    recent_alerts_table_->verticalHeader()->setVisible(false);
    alerts_layout->addWidget(recent_alerts_table_);
    layout->addWidget(alerts_group);

    BuildModelMetricsSection(layout);
}

void DashboardPage::BuildModelMetricsSection(QVBoxLayout* parent_layout) {
    QFile file(QStringLiteral(":/data/model_metrics.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return;
    }
    const QJsonObject obj = doc.object();

    auto* group = new QGroupBox(QStringLiteral("模型性能指标"));
    auto* layout = new QVBoxLayout(group);

    // Model info + overall metrics
    const QString model_type = obj.value(QStringLiteral("model_type")).toString();
    const double accuracy = obj.value(QStringLiteral("test_accuracy")).toDouble();
    const double macro_f1 = obj.value(QStringLiteral("test_macro_f1")).toDouble();
    const double macro_recall = obj.value(QStringLiteral("test_macro_recall")).toDouble();

    auto* info_label = new QLabel(
        QStringLiteral("模型: %1").arg(model_type));
    info_label->setObjectName(QStringLiteral("pageSubtitle"));
    layout->addWidget(info_label);

    auto* metrics_row = new QHBoxLayout();
    metrics_row->setSpacing(24);

    auto make_metric = [&](const QString& name, double value) {
        auto* box = new QVBoxLayout();
        auto* name_label = new QLabel(name);
        name_label->setAlignment(Qt::AlignCenter);
        auto* value_label = new QLabel(QStringLiteral("%1%").arg(value * 100.0, 0, 'f', 2));
        value_label->setAlignment(Qt::AlignCenter);
        QFont font = value_label->font();
        font.setPointSize(18);
        font.setBold(true);
        value_label->setFont(font);
        if (value >= 0.95) {
            value_label->setStyleSheet(QStringLiteral("color: #17663a;"));
        } else if (value >= 0.85) {
            value_label->setStyleSheet(QStringLiteral("color: #b54708;"));
        }
        box->addWidget(value_label);
        box->addWidget(name_label);
        metrics_row->addLayout(box);
    };

    make_metric(QStringLiteral("准确率 (Accuracy)"), accuracy);
    make_metric(QStringLiteral("宏 F1 (Macro F1)"), macro_f1);
    make_metric(QStringLiteral("宏召回率 (Macro Recall)"), macro_recall);
    metrics_row->addStretch();
    layout->addLayout(metrics_row);

    // Per-class recall
    static const QString class_names[] = {
        QStringLiteral("N (正常)"),
        QStringLiteral("S (室上性)"),
        QStringLiteral("V (室性)"),
        QStringLiteral("F (融合)"),
        QStringLiteral("Q (未知)"),
    };

    const QJsonArray per_class = obj.value(QStringLiteral("test_per_class_recall")).toArray();
    if (per_class.size() == 5) {
        auto* recall_label = new QLabel(QStringLiteral("各类别召回率:"));
        recall_label->setObjectName(QStringLiteral("tipLabel"));
        layout->addWidget(recall_label);

        auto* recall_table = new QTableWidget(1, 5, group);
        recall_table->setMaximumHeight(60);
        recall_table->verticalHeader()->hide();
        recall_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        QStringList headers;
        for (int i = 0; i < 5; ++i) {
            headers << class_names[i];
        }
        recall_table->setHorizontalHeaderLabels(headers);
        recall_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        for (int i = 0; i < 5; ++i) {
            const double val = per_class[i].toDouble();
            auto* item = new QTableWidgetItem(
                QStringLiteral("%1%").arg(val * 100.0, 0, 'f', 1));
            item->setTextAlignment(Qt::AlignCenter);
            if (val >= 0.95) {
                item->setForeground(QColor(0x17, 0x66, 0x3a));
            } else if (val < 0.85) {
                item->setForeground(QColor(0xB4, 0x23, 0x18));
            }
            recall_table->setItem(0, i, item);
        }
        layout->addWidget(recall_table);
    }

    // Confusion matrix
    const QJsonArray cm = obj.value(QStringLiteral("confusion_matrix")).toArray();
    if (cm.size() == 5) {
        auto* cm_label = new QLabel(QStringLiteral("混淆矩阵 (行=真实, 列=预测):"));
        cm_label->setObjectName(QStringLiteral("tipLabel"));
        layout->addWidget(cm_label);

        auto* cm_table = new QTableWidget(5, 5, group);
        cm_table->setMaximumHeight(180);
        cm_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        QStringList headers;
        for (int i = 0; i < 5; ++i) {
            headers << class_names[i];
        }
        cm_table->setHorizontalHeaderLabels(headers);
        cm_table->setVerticalHeaderLabels(headers);
        cm_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        for (int r = 0; r < 5; ++r) {
            const QJsonArray row = cm[r].toArray();
            for (int c = 0; c < 5 && c < row.size(); ++c) {
                const int val = row[c].toInt();
                auto* item = new QTableWidgetItem(QString::number(val));
                item->setTextAlignment(Qt::AlignCenter);
                if (r == c) {
                    item->setBackground(QColor(0xEE, 0xF8, 0xF1));
                    item->setForeground(QColor(0x17, 0x66, 0x3a));
                    QFont f = item->font();
                    f.setBold(true);
                    item->setFont(f);
                } else if (val > 0) {
                    item->setForeground(QColor(0x99, 0x99, 0x99));
                }
                cm_table->setItem(r, c, item);
            }
        }
        layout->addWidget(cm_table);
    }

    parent_layout->addWidget(group);
}

void DashboardPage::UpdateSummaryCards() {
    if (patient_count_label_ != nullptr) {
        patient_count_label_->setText(QString::number(patient_count_));
    }
    if (record_count_label_ != nullptr) {
        record_count_label_->setText(QString::number(record_count_));
    }
    if (alert_count_label_ != nullptr) {
        alert_count_label_->setText(QString::number(alert_count_));
    }
    if (unconfirmed_count_label_ != nullptr) {
        unconfirmed_count_label_->setText(QString::number(unconfirmed_count_));
    }
}
