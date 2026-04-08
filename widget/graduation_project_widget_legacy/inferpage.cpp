#include "inferpage.h"
#include "ui_inferpage.h"

#include <QComboBox>
#include <QDateTime>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

#include "ecg_wave_widget.h"
#include "logger.h"
#include "tcpmgr.h"

namespace {

QString CurrentTimeText() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

QString ConnectionStateName(ClientState state) {
    switch (state) {
        case ClientState::Connected:
            return QStringLiteral("connected");
        case ClientState::Connecting:
            return QStringLiteral("connecting");
        case ClientState::Disconnected:
            return QStringLiteral("disconnected");
    }
    return QStringLiteral("disconnected");
}

void RefreshWidgetStyle(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

int ConfidencePercent(const QString& text) {
    QString normalized = text.trimmed();
    normalized.remove('%');

    bool ok = false;
    double value = normalized.toDouble(&ok);
    if (!ok) {
        return 0;
    }
    if (value <= 1.0) {
        value *= 100.0;
    }
    return qBound(0, static_cast<int>(value + 0.5), 100);
}

}  // namespace

InferPage::InferPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::InferPage) {
    ui->setupUi(this);
    CreateDynamicPanels();
    BindSignals();
    ResetResultFields();
    UpdateUiState(ClientState::Disconnected);
    UpdatePatientBanner();

    ui->groupBoxConnection->setTitle(QStringLiteral("边缘推理连接"));
    ui->groupBoxRequest->setTitle(QStringLiteral("手动推理输入"));
    ui->groupBoxResult->setTitle(QStringLiteral("推理结果面板"));
    ui->groupBoxLog->setTitle(QStringLiteral("通信与推理日志"));
    ui->plainTextEditFeatures->setPlaceholderText(
        QStringLiteral("请输入 187 维特征，使用英文逗号、空格或换行分隔。\n"
                       "例如：0.12,0.15,-0.03,..."));
    ui->plainTextEditLog->setMaximumBlockCount(500);
    ui->labelConnectionStateValue->setProperty("statusText", true);
    ui->lineEditAlert->setProperty("resultBadge", true);
    ui->lineEditAlert->setAlignment(Qt::AlignCenter);
    ui->pushButtonConnect->setProperty("buttonRole", QStringLiteral("primary"));
    ui->pushButtonDisconnect->setProperty("buttonRole", QStringLiteral("danger"));
    ui->pushButtonPing->setProperty("buttonRole", QStringLiteral("secondary"));
    ui->pushButtonPredict->setProperty("buttonRole", QStringLiteral("primary"));
    ui->pushButtonFillSample->setProperty("buttonRole", QStringLiteral("secondary"));
    ui->pushButtonClearLog->setProperty("buttonRole", QStringLiteral("secondary"));

    auto* confidence_label = new QLabel(QStringLiteral("置信度进度"), ui->groupBoxResult);
    confidence_progress_bar_ = new QProgressBar(ui->groupBoxResult);
    confidence_progress_bar_->setRange(0, 100);
    confidence_progress_bar_->setFormat(QStringLiteral("%p%"));
    confidence_progress_bar_->setValue(0);
    ui->gridLayoutResult->addWidget(confidence_label, 2, 0);
    ui->gridLayoutResult->addWidget(confidence_progress_bar_, 2, 1, 1, 3);
}

InferPage::~InferPage() {
    delete ui;
}

bool InferPage::HasCurrentPatient() const {
    return has_current_patient_;
}

QString InferPage::CurrentPatientId() const {
    return current_patient_.patient_id;
}

void InferPage::SetCurrentPatient(const PatientInfo& patient_info) {
    current_patient_ = patient_info;
    has_current_patient_ = true;
    UpdatePatientBanner();
}

void InferPage::ClearCurrentPatient() {
    current_patient_ = PatientInfo();
    has_current_patient_ = false;
    UpdatePatientBanner();
}

void InferPage::slot_append_log(const QString& text) {
    AppendLog(text);
}

void InferPage::slot_on_monitor_record_saved(const QString& message) {
    if (!isVisible()) {
        return;
    }
    AppendLog(QStringLiteral("业务侧已保存监测结果：%1").arg(message));
}

void InferPage::slot_on_business_error(const QString& error_text) {
    if (!isVisible()) {
        return;
    }
    AppendLog(QStringLiteral("业务错误: %1").arg(error_text));
}

void InferPage::slot_on_business_connection_closed() {
    if (!isVisible()) {
        return;
    }
    AppendLog(QStringLiteral("业务服务端连接已断开，后续推理结果将不会自动保存。"));
}

void InferPage::BindSignals() {
    connect(ui->pushButtonConnect, &QPushButton::clicked, this, &InferPage::OnConnectClicked);
    connect(ui->pushButtonDisconnect, &QPushButton::clicked, this, &InferPage::OnDisconnectClicked);
    connect(ui->pushButtonPing, &QPushButton::clicked, this, &InferPage::OnPingClicked);
    connect(ui->pushButtonPredict, &QPushButton::clicked, this, &InferPage::OnPredictClicked);
    connect(ui->pushButtonFillSample, &QPushButton::clicked, this, &InferPage::OnFillSampleClicked);
    connect(ui->pushButtonClearLog, &QPushButton::clicked, this, &InferPage::OnClearLogClicked);
    connect(pushButtonLoadSamples_, &QPushButton::clicked, this, &InferPage::OnLoadSamplesClicked);
    connect(pushButtonPlaySample_, &QPushButton::clicked, this, &InferPage::OnPlaySampleClicked);
    connect(pushButtonBack_, &QPushButton::clicked, this, &InferPage::OnBackClicked);

    auto tcp_mgr = TcpMgr::GetInstance().get();
    connect(this, &InferPage::sig_tcp_connect, tcp_mgr, &TcpMgr::slot_tcp_connect);
    connect(this, &InferPage::sig_disconnect, tcp_mgr, &TcpMgr::slot_disconnect);
    connect(this, &InferPage::sig_send_ping, tcp_mgr, &TcpMgr::slot_send_ping);
    connect(this, &InferPage::sig_send_predict, tcp_mgr, &TcpMgr::slot_send_predict);
    connect(this, &InferPage::sig_list_samples, tcp_mgr, &TcpMgr::slot_list_samples);
    connect(this, &InferPage::sig_play_sample, tcp_mgr, &TcpMgr::slot_play_sample);

    connect(tcp_mgr, &TcpMgr::sig_con_success, this, &InferPage::OnConnectResult);
    connect(tcp_mgr, &TcpMgr::sig_connection_closed, this, &InferPage::OnConnectionClosed);
    connect(tcp_mgr, &TcpMgr::sig_state_changed, this, &InferPage::OnStateChanged);
    connect(tcp_mgr, &TcpMgr::sig_log_message, this, &InferPage::AppendLog);
    connect(tcp_mgr, &TcpMgr::sig_pong, this, &InferPage::OnPong);
    connect(tcp_mgr, &TcpMgr::sig_predict_result, this, &InferPage::OnPredictResult);
    connect(tcp_mgr, &TcpMgr::sig_sample_list_ready, this, &InferPage::OnSampleListReceived);
    connect(tcp_mgr, &TcpMgr::sig_beat_response, this, &InferPage::OnBeatResponse);
    connect(tcp_mgr, &TcpMgr::sig_server_error, this, &InferPage::OnServerError);
    connect(tcp_mgr, &TcpMgr::sig_protocol_error, this, &InferPage::OnProtocolError);
}

void InferPage::CreateDynamicPanels() {
    auto* group_box_patient = new QGroupBox(QStringLiteral("当前监测对象"), this);
    auto* patient_layout = new QHBoxLayout(group_box_patient);
    labelCurrentPatient_ = new QLabel(group_box_patient);
    labelCurrentPatient_->setWordWrap(true);
    labelCurrentPatient_->setObjectName(QStringLiteral("pageSubtitle"));
    pushButtonBack_ = new QPushButton(QStringLiteral("病人管理"), group_box_patient);
    pushButtonBack_->setProperty("buttonRole", QStringLiteral("secondary"));
    patient_layout->addWidget(labelCurrentPatient_, 1);
    patient_layout->addWidget(pushButtonBack_);
    ui->verticalLayoutRoot->insertWidget(0, group_box_patient);

    groupBoxSamples_ = new QGroupBox(QStringLiteral("模拟采集"), this);
    auto* sample_layout = new QHBoxLayout(groupBoxSamples_);
    auto* label_sample = new QLabel(QStringLiteral("样本文件"), groupBoxSamples_);
    comboBoxSamples_ = new QComboBox(groupBoxSamples_);
    comboBoxSamples_->setMinimumContentsLength(26);
    comboBoxSamples_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    pushButtonLoadSamples_ = new QPushButton(QStringLiteral("加载样本列表"), groupBoxSamples_);
    pushButtonLoadSamples_->setProperty("buttonRole", QStringLiteral("secondary"));
    pushButtonPlaySample_ = new QPushButton(QStringLiteral("播放样本"), groupBoxSamples_);
    pushButtonPlaySample_->setProperty("buttonRole", QStringLiteral("primary"));

    sample_layout->addWidget(label_sample);
    sample_layout->addWidget(comboBoxSamples_, 1);
    sample_layout->addWidget(pushButtonLoadSamples_);
    sample_layout->addWidget(pushButtonPlaySample_);
    ui->verticalLayoutRoot->insertWidget(2, groupBoxSamples_);

    auto* group_box_wave = new QGroupBox(QStringLiteral("心电波形"), this);
    auto* wave_layout = new QVBoxLayout(group_box_wave);
    waveWidget_ = new EcgWaveWidget(group_box_wave);
    wave_layout->addWidget(waveWidget_);
    ui->verticalLayoutRoot->insertWidget(3, group_box_wave);

    auto* group_box_history = new QGroupBox(QStringLiteral("推理历史（最近 20 条，点击可回放波形）"), this);
    auto* history_layout = new QVBoxLayout(group_box_history);
    historyTable_ = new QTableWidget(0, 5, group_box_history);
    historyTable_->setHorizontalHeaderLabels({
        QStringLiteral("时间"),
        QStringLiteral("预测类别"),
        QStringLiteral("置信度"),
        QStringLiteral("告警等级"),
        QStringLiteral("来源"),
    });
    historyTable_->horizontalHeader()->setStretchLastSection(true);
    historyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    historyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable_->verticalHeader()->hide();
    historyTable_->setMaximumHeight(240);
    history_layout->addWidget(historyTable_);
    // Insert after wave widget (index 4), before log group box
    ui->verticalLayoutRoot->insertWidget(4, group_box_history);

    connect(historyTable_, &QTableWidget::cellClicked, this, &InferPage::OnHistoryRowClicked);
}

void InferPage::OnConnectClicked() {
    const QString host = ui->lineEditHost->text().trimmed();
    if (host.isEmpty()) {
        gp::logging::Logger::Instance().WarningText("用户输入为空，连接被拒绝");
        QMessageBox::warning(this, QStringLiteral("连接失败"), QStringLiteral("服务器 IP 不能为空。"));
        return;
    }

    ServerInfo server_info;
    server_info.host = host;
    server_info.port = static_cast<quint16>(ui->spinBoxPort->value());
    emit sig_tcp_connect(server_info);
}

void InferPage::OnDisconnectClicked() {
    emit sig_disconnect();
}

void InferPage::OnPingClicked() {
    emit sig_send_ping();
}

void InferPage::OnPredictClicked() {
    QString error_message;
    if (!ParseFeatureValues(&manual_values_, &error_message)) {
        gp::logging::Logger::Instance().WarningText(
            QString("预测输入不合法: %1").arg(error_message).toUtf8().toStdString());
        QMessageBox::warning(this, QStringLiteral("输入不合法"), error_message);
        return;
    }

    const QString payload = BuildCsvText(manual_values_);
    waveWidget_->setBeat(manual_values_, QStringLiteral("normal"));
    ApplyAlertStyle(QStringLiteral("normal"));
    emit sig_send_predict(payload);
}

void InferPage::OnFillSampleClicked() {
    QVector<double> values;
    values.reserve(187);

    // 这组数据不是训练样本，只是为了先把联调链路跑通。
    for (int i = 0; i < 187; ++i) {
        const double x = static_cast<double>(i) / 186.0;
        double value = 0.02 * std::sin(6.28318530718 * 2.0 * x);

        if (i >= 80 && i <= 86) {
            value += 0.85 - 0.12 * std::abs(83 - i);
        } else if (i >= 70 && i < 80) {
            value -= 0.03;
        } else if (i > 86 && i <= 108) {
            value += 0.18 * (1.0 - std::abs(97 - i) / 11.0);
        }

        values.push_back(value);
    }

    manual_values_ = values;
    ui->plainTextEditFeatures->setPlainText(BuildCsvText(values));
    ResetResultFields();
    waveWidget_->setBeat(values, QStringLiteral("normal"));
    AppendLog(QStringLiteral("本地: 已填充一组 187 维演示特征"));
}

void InferPage::OnLoadSamplesClicked() {
    emit sig_list_samples();
}

void InferPage::OnPlaySampleClicked() {
    const QString sample_name = comboBoxSamples_->currentText().trimmed();
    if (sample_name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("播放失败"), QStringLiteral("请先加载并选择一个样本。"));
        return;
    }

    emit sig_play_sample(sample_name);
}

void InferPage::OnClearLogClicked() {
    ui->plainTextEditLog->clear();
}

void InferPage::OnBackClicked() {
    emit sig_back_to_patients();
}

void InferPage::OnConnectResult(bool bsuccess) {
    if (!bsuccess) {
        gp::logging::Logger::Instance().WarningText("客户端连接失败");
        QMessageBox::warning(this,
                             QStringLiteral("连接失败"),
                             QStringLiteral("无法连接服务器，请检查 IP、端口和服务是否启动。"));
        return;
    }

    emit sig_list_samples();
}

void InferPage::OnConnectionClosed() {
    ResetResultFields();
    UpdateUiState(ClientState::Disconnected);
}

void InferPage::OnStateChanged(ClientState state) {
    UpdateUiState(state);
}

void InferPage::OnPong() {
    AppendLog(QStringLiteral("本地: 心跳校验成功"));
}

void InferPage::OnPredictResult(const PredictResult& result) {
    UpdateResultFields(result.pred_label, result.confidence, result.alert_level, result.latency_ms);
    waveWidget_->setAlertLevel(result.alert_level);
    AddHistoryEntry(result.pred_label, result.confidence, result.alert_level,
                    result.latency_ms, QStringLiteral("手动输入"), manual_values_);
    NotifyAlarmIfNeeded(result.alert_level, result.pred_label);
    SubmitMonitorRecord(result.pred_label,
                        result.confidence,
                        result.alert_level,
                        result.latency_ms,
                        QStringLiteral("manual_input"),
                        QString(),
                        QString());
}

void InferPage::OnSampleListReceived(const QStringList& samples) {
    comboBoxSamples_->clear();
    comboBoxSamples_->addItems(samples);
    UpdateUiState(TcpMgr::GetInstance().get()->state());

    if (samples.isEmpty()) {
        AppendLog(QStringLiteral("本地: 服务端样本列表为空，请检查 --sample-dir 是否配置。"));
        return;
    }

    AppendLog(QStringLiteral("本地: 已加载 %1 个样本，可直接播放。").arg(samples.size()));
}

void InferPage::OnBeatResponse(const BeatResponse& response) {
    UpdateResultFields(response.pred_label, response.confidence, response.alert_level, response.latency_ms);
    ui->plainTextEditFeatures->setPlainText(BuildCsvText(response.values));
    waveWidget_->setBeat(response.values, response.alert_level);
    AddHistoryEntry(response.pred_label, response.confidence, response.alert_level,
                    response.latency_ms, response.sample_name, response.values);
    manual_values_.clear();

    const int sample_index = comboBoxSamples_->findText(response.sample_name);
    if (sample_index >= 0) {
        comboBoxSamples_->setCurrentIndex(sample_index);
    }

    AppendLog(QStringLiteral("本地: 已播放样本 %1，真实标签=%2。")
                  .arg(response.sample_name, response.true_label));
    NotifyAlarmIfNeeded(response.alert_level, response.pred_label);
    SubmitMonitorRecord(response.pred_label,
                        response.confidence,
                        response.alert_level,
                        response.latency_ms,
                        QStringLiteral("sample_playback"),
                        response.sample_name,
                        response.true_label);
}

void InferPage::OnServerError(const QString& text) {
    AppendLog(QStringLiteral("错误: %1").arg(text));
}

void InferPage::OnProtocolError(const QString& text) {
    AppendLog(QStringLiteral("协议错误: %1").arg(text));
}

void InferPage::AppendLog(const QString& text) {
    ui->plainTextEditLog->appendPlainText(QStringLiteral("[%1] %2").arg(CurrentTimeText(), text));
}

void InferPage::UpdateUiState(ClientState state) {
    const bool is_connected = state == ClientState::Connected;
    const bool is_connecting = state == ClientState::Connecting;
    const bool has_samples = comboBoxSamples_ != nullptr && comboBoxSamples_->count() > 0;

    ui->pushButtonConnect->setEnabled(!is_connected && !is_connecting);
    ui->pushButtonDisconnect->setEnabled(is_connected || is_connecting);
    ui->pushButtonPing->setEnabled(is_connected);
    ui->pushButtonPredict->setEnabled(is_connected);
    ui->lineEditHost->setEnabled(!is_connected && !is_connecting);
    ui->spinBoxPort->setEnabled(!is_connected && !is_connecting);

    pushButtonLoadSamples_->setEnabled(is_connected);
    pushButtonPlaySample_->setEnabled(is_connected && has_samples);
    comboBoxSamples_->setEnabled(is_connected && has_samples);

    if (is_connected) {
        ui->labelConnectionStateValue->setText(QStringLiteral("已连接"));
    } else if (is_connecting) {
        ui->labelConnectionStateValue->setText(QStringLiteral("连接中"));
    } else {
        ui->labelConnectionStateValue->setText(QStringLiteral("未连接"));
    }
    ui->labelConnectionStateValue->setProperty("connectionState", ConnectionStateName(state));
    RefreshWidgetStyle(ui->labelConnectionStateValue);
}

void InferPage::UpdatePatientBanner() {
    if (!has_current_patient_) {
        labelCurrentPatient_->setText(QStringLiteral("当前监测对象：默认监测通道（未绑定具体病人）。系统可直接显示波形和推理结果，如需绑定具体病人，可前往病人管理页面。"));
        return;
    }

    labelCurrentPatient_->setText(
        QStringLiteral("当前监测病人：%1 - %2，%3 岁，%4，联系电话：%5")
            .arg(current_patient_.patient_id, current_patient_.name)
            .arg(current_patient_.age)
            .arg(current_patient_.gender, current_patient_.phone));
}

void InferPage::SubmitMonitorRecord(const QString& pred_label,
                                    const QString& confidence,
                                    const QString& alert_level,
                                    const QString& latency_ms,
                                    const QString& source,
                                    const QString& sample_name,
                                    const QString& true_label) {
    if (!has_current_patient_) {
        AppendLog(QStringLiteral("未选中病人，本次推理结果未写入业务记录。"));
        return;
    }

    MonitorRecordInfo record_info;
    record_info.patient_id = current_patient_.patient_id;
    record_info.pred_label = pred_label;
    record_info.confidence = confidence;
    record_info.alert_level = alert_level;
    record_info.latency_ms = latency_ms;
    record_info.source = source;
    record_info.sample_name = sample_name;
    record_info.true_label = true_label;
    emit sig_add_monitor_record(record_info);
}

void InferPage::NotifyAlarmIfNeeded(const QString& alert_level, const QString& pred_label) {
    if (alert_level == QStringLiteral("critical")) {
        AppendLog(QStringLiteral("严重告警：检测到高风险心拍，预测类别=%1。" ).arg(pred_label));
        QMessageBox::warning(this,
                             QStringLiteral("严重告警"),
                             QStringLiteral("当前结果触发严重告警，预测类别：%1。\n该结果会自动写入报警记录。").arg(pred_label));
        return;
    }

    if (alert_level == QStringLiteral("warning")) {
        AppendLog(QStringLiteral("预警：检测到异常心拍，预测类别=%1，该结果会写入报警记录。")
                      .arg(pred_label));
    }
}

bool InferPage::ParseFeatureValues(QVector<double>* values, QString* error_message) const {
    const QString text = ui->plainTextEditFeatures->toPlainText().trimmed();
    if (text.isEmpty()) {
        *error_message = QStringLiteral("请先输入 187 维特征。");
        return false;
    }

    const QStringList tokens = text.split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts);
    if (tokens.size() != 187) {
        *error_message = QStringLiteral("特征数量不正确，当前为 %1，要求为 187。").arg(tokens.size());
        return false;
    }

    QVector<double> parsed_values;
    parsed_values.reserve(tokens.size());
    const QLocale locale = QLocale::c();

    for (int i = 0; i < tokens.size(); ++i) {
        bool ok = false;
        const double value = locale.toDouble(tokens.at(i), &ok);
        if (!ok) {
            *error_message = QStringLiteral("第 %1 个特征不是合法数字。").arg(i + 1);
            return false;
        }
        parsed_values.push_back(value);
    }

    *values = std::move(parsed_values);
    return true;
}

QString InferPage::BuildCsvText(const QVector<double>& values) {
    QStringList normalized_values;
    normalized_values.reserve(values.size());
    for (double value : values) {
        normalized_values.push_back(QString::number(value, 'f', 6));
    }
    return normalized_values.join(',');
}

void InferPage::UpdateResultFields(const QString& pred,
                                   const QString& confidence,
                                   const QString& alert_level,
                                   const QString& latency_ms) {
    ui->lineEditPred->setText(pred);
    ui->lineEditConf->setText(confidence);
    ui->lineEditAlert->setText(alert_level);
    ui->lineEditLatency->setText(latency_ms);
    if (confidence_progress_bar_ != nullptr) {
        confidence_progress_bar_->setValue(ConfidencePercent(confidence));
    }
    ApplyAlertStyle(alert_level);
}

void InferPage::ApplyAlertStyle(const QString& alert_level) {
    QString normalized_level = alert_level.trimmed().toLower();
    if (normalized_level.isEmpty()) {
        normalized_level = QStringLiteral("normal");
    }
    if (normalized_level != QStringLiteral("critical") &&
        normalized_level != QStringLiteral("warning")) {
        normalized_level = QStringLiteral("normal");
    }
    ui->lineEditAlert->setProperty("alertLevel", normalized_level);
    RefreshWidgetStyle(ui->lineEditAlert);
}

void InferPage::ResetResultFields() {
    ui->lineEditPred->clear();
    ui->lineEditConf->clear();
    ui->lineEditAlert->clear();
    ui->lineEditLatency->clear();
    if (confidence_progress_bar_ != nullptr) {
        confidence_progress_bar_->setValue(0);
    }
    ApplyAlertStyle(QStringLiteral("normal"));
}

void InferPage::AddHistoryEntry(const QString& pred_label, const QString& confidence,
                                const QString& alert_level, const QString& latency_ms,
                                const QString& source, const QVector<double>& values) {
    BeatHistoryEntry entry;
    entry.time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    entry.pred_label = pred_label;
    entry.confidence = confidence;
    entry.alert_level = alert_level;
    entry.latency_ms = latency_ms;
    entry.source = source;
    entry.values = values;

    history_entries_.prepend(entry);
    if (history_entries_.size() > kMaxHistoryEntries) {
        history_entries_.resize(kMaxHistoryEntries);
    }

    // Rebuild table
    historyTable_->setRowCount(history_entries_.size());
    for (int i = 0; i < history_entries_.size(); ++i) {
        const auto& e = history_entries_[i];
        historyTable_->setItem(i, 0, new QTableWidgetItem(e.time));
        historyTable_->setItem(i, 1, new QTableWidgetItem(e.pred_label));
        historyTable_->setItem(i, 2, new QTableWidgetItem(e.confidence));
        historyTable_->setItem(i, 3, new QTableWidgetItem(e.alert_level));
        historyTable_->setItem(i, 4, new QTableWidgetItem(e.source));

        if (e.alert_level == QStringLiteral("critical")) {
            historyTable_->item(i, 3)->setBackground(QColor(80, 24, 36));
            historyTable_->item(i, 3)->setForeground(QColor(252, 165, 165));
        } else if (e.alert_level == QStringLiteral("warning")) {
            historyTable_->item(i, 3)->setBackground(QColor(87, 52, 22));
            historyTable_->item(i, 3)->setForeground(QColor(252, 211, 77));
        } else {
            historyTable_->item(i, 3)->setBackground(QColor(20, 83, 45));
            historyTable_->item(i, 3)->setForeground(QColor(110, 231, 183));
        }
    }
    historyTable_->resizeColumnsToContents();
}

void InferPage::OnHistoryRowClicked(int row) {
    if (row < 0 || row >= history_entries_.size()) {
        return;
    }
    const auto& entry = history_entries_[row];
    if (entry.values.isEmpty()) {
        AppendLog(QStringLiteral("该历史记录无波形数据，无法回放。"));
        return;
    }
    waveWidget_->setBeat(entry.values, entry.alert_level);
    ui->plainTextEditFeatures->setPlainText(BuildCsvText(entry.values));
    UpdateResultFields(entry.pred_label, entry.confidence, entry.alert_level, entry.latency_ms);
    AppendLog(QStringLiteral("回放历史记录：%1 - %2").arg(entry.time, entry.pred_label));
}












