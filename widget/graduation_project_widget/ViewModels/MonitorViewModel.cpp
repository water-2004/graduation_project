#include "MonitorViewModel.h"

#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>
#include <QtGlobal>

#include <cmath>

namespace {

QString EdgeStateName(ClientState state) {
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

QString BuildPlaybackName(const QString& source, const QString& sample_name) {
    if (!sample_name.isEmpty()) {
        return sample_name;
    }
    if (source == QStringLiteral("manual_input")) {
        return QStringLiteral("手动输入片段");
    }
    if (source.isEmpty()) {
        return QStringLiteral("未命名片段");
    }
    return source;
}


QString BuildAlertToastMessage(const QString& patient_name,
                               const QString& pred_label,
                               const QString& confidence,
                               bool critical) {
    const QString display_name = patient_name.isEmpty()
        ? QString::fromUtf8(u8"默认监测对象")
        : patient_name;
    return critical
        ? QString::fromUtf8(u8"严重告警：%1 检测到 %2，置信度 %3，请立即处理。").arg(display_name, pred_label, confidence)
        : QString::fromUtf8(u8"监测提示：%1 检测到 %2，置信度 %3，请继续观察。").arg(display_name, pred_label, confidence);
}

}  // namespace

MonitorViewModel::MonitorViewModel(NetworkService* network_service, QObject* parent)
    : QObject(parent)
    , network_service_(network_service)
    , ecg_stream_(this)
    , history_model_(this) {
    playback_timer_.setInterval(35);
    connect(&playback_timer_, &QTimer::timeout, this, [this]() {
        if (playback_points_.isEmpty()) {
            playback_timer_.stop();
            if (is_playing_) {
                is_playing_ = false;
                emit playbackStateChanged();
            }
            return;
        }

        playback_visible_count_ = qMin(playback_visible_count_ + playback_step_, playback_points_.size());
        UpdatePlaybackFrame();

        if (playback_visible_count_ >= playback_points_.size()) {
            playback_timer_.stop();
            if (is_playing_) {
                is_playing_ = false;
                emit playbackStateChanged();
            }
        }
    });

    connect(network_service_, &NetworkService::stateChanged, this, [this](ClientState state) {
        UpdateEdgeState(state);

        if (state == ClientState::Connected && auto_load_samples_after_connect_) {
            auto_load_samples_after_connect_ = false;
            network_service_->loadSamples();
        }

        if (state == ClientState::Disconnected) {
            sample_names_.clear();
            if (selected_sample_index_ != -1) {
                selected_sample_index_ = -1;
                emit selectedSampleIndexChanged();
            }
            emit sampleNamesChanged();
        }
    });
    connect(network_service_, &NetworkService::sampleListReady, this, [this](const QStringList& samples) {
        sample_names_ = samples;
        if (sample_names_.isEmpty()) {
            if (selected_sample_index_ != -1) {
                selected_sample_index_ = -1;
                emit selectedSampleIndexChanged();
            }
            emit sampleNamesChanged();
            emit toastRequested(QStringLiteral("当前没有可播放的样本，请检查边缘服务启动时是否传入 --sample-dir。"),
                               QStringLiteral("warning"));
            return;
        }

        if (selected_sample_index_ < 0 || selected_sample_index_ >= sample_names_.size()) {
            selected_sample_index_ = 0;
            emit selectedSampleIndexChanged();
        }
        emit sampleNamesChanged();
    });
    connect(network_service_, &NetworkService::predictResult, this, [this](const PredictResult& result) {
        OnPredictResult(result, manual_values_, QStringLiteral("manual_input"), QString(), QString());
    });
    connect(network_service_, &NetworkService::beatResponse, this, [this](const BeatResponse& response) {
        const QVariantList wave_points = ToVariantList(response.values);
        const QString playback_name = response.sample_name.isEmpty()
            ? QStringLiteral("样本片段")
            : response.sample_name;

        UpdateResult(response.pred_label, response.confidence, response.alert_level, response.latency_ms);
        SetPlaybackSource(wave_points, response.alert_level, playback_name, true);

        MonitorHistoryItem item;
        item.time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
        item.predLabel = response.pred_label;
        item.confidence = response.confidence;
        item.alertLevel = response.alert_level;
        item.latencyMs = response.latency_ms;
        item.source = playback_name;
        item.wavePoints = wave_points;
        history_model_.prependItem(item);

        if (response.alert_level == QStringLiteral("critical")) {
            emit toastRequested(BuildAlertToastMessage(currentPatientName(),
                                                      response.pred_label,
                                                      response.confidence,
                                                      true),
                               QStringLiteral("critical"));
        } else if (response.alert_level == QStringLiteral("warning")) {
            emit toastRequested(BuildAlertToastMessage(currentPatientName(),
                                                      response.pred_label,
                                                      response.confidence,
                                                      false),
                               QStringLiteral("warning"));
        }

        MonitorRecordInfo record;
        record.patient_id = current_patient_.patient_id;
        record.pred_label = response.pred_label;
        record.confidence = response.confidence;
        record.alert_level = response.alert_level;
        record.latency_ms = response.latency_ms;
        record.source = QStringLiteral("sample_playback");
        record.sample_name = response.sample_name;
        record.true_label = response.true_label;
        emit monitorRecordReady(record);
    });
    connect(network_service_, &NetworkService::serverError, this, [this](const QString& text) {
        emit toastRequested(text, QStringLiteral("critical"));
    });
    connect(network_service_, &NetworkService::protocolError, this, [this](const QString& text) {
        emit toastRequested(text, QStringLiteral("warning"));
    });
    connect(network_service_, &NetworkService::connectionClosed, this, [this]() {
        UpdateEdgeState(ClientState::Disconnected);
    });

    UpdateEdgeState(network_service_ != nullptr ? network_service_->state() : ClientState::Disconnected);
}

QString MonitorViewModel::edgeState() const { return edge_state_; }
QString MonitorViewModel::edgeStateText() const { return edge_state_text_; }
QString MonitorViewModel::edgeHost() const { return edge_host_; }

void MonitorViewModel::setEdgeHost(const QString& host) {
    if (edge_host_ != host) {
        edge_host_ = host;
        emit edgeConfigChanged();
    }
}

int MonitorViewModel::edgePort() const { return edge_port_; }

void MonitorViewModel::setEdgePort(int port) {
    if (edge_port_ != port) {
        edge_port_ = port;
        emit edgeConfigChanged();
    }
}

QString MonitorViewModel::currentPatientId() const { return current_patient_.patient_id; }

QString MonitorViewModel::currentPatientName() const {
    return has_current_patient_ ? current_patient_.name : QStringLiteral("默认监测通道");
}

QString MonitorViewModel::currentPatientSummary() const {
    if (!has_current_patient_) {
        return QStringLiteral("未绑定具体病人，系统仍可演示实时波形与推理结果。");
    }
    return QStringLiteral("%1 · %2岁 · %3 · %4")
        .arg(current_patient_.gender)
        .arg(current_patient_.age)
        .arg(current_patient_.phone, current_patient_.remark);
}

bool MonitorViewModel::hasCurrentPatient() const { return has_current_patient_; }
QStringList MonitorViewModel::sampleNames() const { return sample_names_; }
int MonitorViewModel::selectedSampleIndex() const { return selected_sample_index_; }

void MonitorViewModel::setSelectedSampleIndex(int index) {
    if (selected_sample_index_ != index) {
        selected_sample_index_ = index;
        emit selectedSampleIndexChanged();
    }
}

QString MonitorViewModel::predLabel() const { return pred_label_; }
QString MonitorViewModel::confidenceText() const { return confidence_text_; }
QString MonitorViewModel::alertLevel() const { return alert_level_; }
QString MonitorViewModel::latencyText() const { return latency_text_; }
int MonitorViewModel::confidencePercent() const { return confidence_percent_; }
EcgDataStream* MonitorViewModel::ecgStream() { return &ecg_stream_; }
HistoryModel* MonitorViewModel::historyModel() { return &history_model_; }
bool MonitorViewModel::playbackAvailable() const { return !playback_points_.isEmpty(); }
bool MonitorViewModel::isPlaying() const { return is_playing_; }

double MonitorViewModel::playbackProgress() const {
    if (playback_points_.isEmpty()) {
        return 0.0;
    }
    return qBound(0.0,
                  static_cast<double>(playback_visible_count_) / static_cast<double>(playback_points_.size()),
                  1.0);
}

QString MonitorViewModel::playbackProgressText() const {
    if (playback_points_.isEmpty()) {
        return QStringLiteral("0 / 0");
    }
    return QStringLiteral("%1 / %2")
        .arg(qMin(playback_visible_count_, playback_points_.size()))
        .arg(playback_points_.size());
}

QString MonitorViewModel::currentPlaybackName() const {
    return current_playback_name_.isEmpty() ? QStringLiteral("等待波形回放") : current_playback_name_;
}

void MonitorViewModel::setCurrentPatient(const PatientInfo& patient) {
    current_patient_ = patient;
    has_current_patient_ = !patient.patient_id.isEmpty();
    emit patientChanged();
}

void MonitorViewModel::clearCurrentPatient() {
    current_patient_ = PatientInfo();
    has_current_patient_ = false;
    history_model_.clear();
    ecg_stream_.clear();
    pred_label_.clear();
    confidence_text_.clear();
    latency_text_.clear();
    confidence_percent_ = 0;
    alert_level_ = QStringLiteral("normal");
    playback_timer_.stop();
    playback_points_.clear();
    playback_alert_level_ = QStringLiteral("normal");
    current_playback_name_.clear();
    playback_visible_count_ = 0;
    playback_step_ = 1;
    is_playing_ = false;
    emit patientChanged();
    emit resultChanged();
    emit playbackStateChanged();
}

void MonitorViewModel::connectEdge() {
    auto_load_samples_after_connect_ = true;
    network_service_->connectEdge(edge_host_, edge_port_);
}
void MonitorViewModel::disconnectEdge() {
    auto_load_samples_after_connect_ = false;
    if (!sample_names_.isEmpty()) {
        sample_names_.clear();
        emit sampleNamesChanged();
    }
    if (selected_sample_index_ != -1) {
        selected_sample_index_ = -1;
        emit selectedSampleIndexChanged();
    }
    UpdateEdgeState(ClientState::Disconnected);
    network_service_->disconnectEdge();
}
void MonitorViewModel::sendPing() { network_service_->sendPing(); }
void MonitorViewModel::loadSamples() {
    auto_load_samples_after_connect_ = false;
    network_service_->loadSamples();
}

void MonitorViewModel::playSelectedSample() {
    if (selected_sample_index_ < 0 || selected_sample_index_ >= sample_names_.size()) {
        emit toastRequested(QStringLiteral("请先加载并选择样本。"), QStringLiteral("warning"));
        return;
    }

    current_playback_name_ = sample_names_.at(selected_sample_index_);
    emit playbackStateChanged();
    network_service_->playSample(current_playback_name_);
}

QString MonitorViewModel::generateDemoInput() const {
    QVector<double> values;
    values.reserve(187);
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
    return BuildCsvText(values);
}

void MonitorViewModel::sendPredict(const QString& feature_text) {
    QString error_text;
    QVector<double> values;
    if (!ParseFeatureValues(feature_text, &values, &error_text)) {
        emit toastRequested(error_text, QStringLiteral("warning"));
        return;
    }

    manual_values_ = values;
    current_playback_name_ = QStringLiteral("手动输入片段");
    emit playbackStateChanged();
    ecg_stream_.setPoints(ToVariantList(values), QStringLiteral("normal"));
    network_service_->sendPredict(BuildCsvText(values));
}

void MonitorViewModel::replayHistory(int index) {
    const MonitorHistoryItem item = history_model_.itemAt(index);
    if (item.wavePoints.isEmpty()) {
        return;
    }

    UpdateResult(item.predLabel, item.confidence, item.alertLevel, item.latencyMs);
    SetPlaybackSource(item.wavePoints,
                      item.alertLevel,
                      item.source.isEmpty() ? QStringLiteral("历史片段 %1").arg(index + 1) : item.source,
                      true);
}

void MonitorViewModel::togglePlayback() {
    if (playback_points_.isEmpty()) {
        emit toastRequested(QStringLiteral("当前还没有可回放的波形片段。"), QStringLiteral("warning"));
        return;
    }

    if (is_playing_) {
        playback_timer_.stop();
        is_playing_ = false;
        emit playbackStateChanged();
        return;
    }

    StartPlayback(playback_visible_count_ >= playback_points_.size());
}

void MonitorViewModel::stopPlayback() {
    playback_timer_.stop();
    is_playing_ = false;
    playback_visible_count_ = 0;
    UpdatePlaybackFrame();
}

void MonitorViewModel::seekPlayback(double progress) {
    if (playback_points_.isEmpty()) {
        return;
    }

    const double bounded = qBound(0.0, progress, 1.0);
    playback_timer_.stop();
    is_playing_ = false;

    int target_count = static_cast<int>(bounded * playback_points_.size() + 0.5);
    if (bounded > 0.0 && target_count == 0) {
        target_count = 1;
    }
    playback_visible_count_ = qBound(0, target_count, playback_points_.size());
    UpdatePlaybackFrame();
}

QString MonitorViewModel::BuildCsvText(const QVector<double>& values) {
    QStringList parts;
    parts.reserve(values.size());
    for (double value : values) {
        parts.push_back(QString::number(value, 'f', 6));
    }
    return parts.join(',');
}

bool MonitorViewModel::ParseFeatureValues(const QString& text,
                                          QVector<double>* values,
                                          QString* error_text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        *error_text = QStringLiteral("请输入 187 维特征。");
        return false;
    }

    const QStringList tokens = trimmed.split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts);
    if (tokens.size() != 187) {
        *error_text = QStringLiteral("特征数量错误，当前为 %1，需要 187。").arg(tokens.size());
        return false;
    }

    const QLocale locale = QLocale::c();
    QVector<double> parsed;
    parsed.reserve(tokens.size());
    for (int i = 0; i < tokens.size(); ++i) {
        bool ok = false;
        const double value = locale.toDouble(tokens.at(i), &ok);
        if (!ok) {
            *error_text = QStringLiteral("第 %1 个特征不是合法数字。").arg(i + 1);
            return false;
        }
        parsed.push_back(value);
    }

    *values = parsed;
    return true;
}

QVariantList MonitorViewModel::ToVariantList(const QVector<double>& values) {
    QVariantList list;
    list.reserve(values.size());
    for (double value : values) {
        list.push_back(value);
    }
    return list;
}

int MonitorViewModel::ConfidencePercent(const QString& text) {
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

void MonitorViewModel::OnPredictResult(const PredictResult& result,
                                       const QVector<double>& values,
                                       const QString& source,
                                       const QString& sample_name,
                                       const QString& true_label) {
    const QVariantList wave_points = ToVariantList(values);
    const QString playback_name = BuildPlaybackName(source, sample_name);

    UpdateResult(result.pred_label, result.confidence, result.alert_level, result.latency_ms);
    SetPlaybackSource(wave_points, result.alert_level, playback_name, true);

    MonitorHistoryItem item;
    item.time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    item.predLabel = result.pred_label;
    item.confidence = result.confidence;
    item.alertLevel = result.alert_level;
    item.latencyMs = result.latency_ms;
    item.source = playback_name;
    item.wavePoints = wave_points;
    history_model_.prependItem(item);

    if (result.alert_level == QStringLiteral("critical")) {
        emit toastRequested(BuildAlertToastMessage(currentPatientName(),
                                                  result.pred_label,
                                                  result.confidence,
                                                  true),
                           QStringLiteral("critical"));
    } else if (result.alert_level == QStringLiteral("warning")) {
        emit toastRequested(BuildAlertToastMessage(currentPatientName(),
                                                  result.pred_label,
                                                  result.confidence,
                                                  false),
                           QStringLiteral("warning"));
    }

    MonitorRecordInfo record;
    record.patient_id = current_patient_.patient_id;
    record.pred_label = result.pred_label;
    record.confidence = result.confidence;
    record.alert_level = result.alert_level;
    record.latency_ms = result.latency_ms;
    record.source = source;
    record.sample_name = sample_name;
    record.true_label = true_label;
    emit monitorRecordReady(record);
}

void MonitorViewModel::SetPlaybackSource(const QVariantList& points,
                                         const QString& alert_level,
                                         const QString& playback_name,
                                         bool auto_start) {
    playback_timer_.stop();
    is_playing_ = false;
    playback_points_ = points;
    playback_alert_level_ = alert_level.isEmpty() ? QStringLiteral("normal") : alert_level;
    current_playback_name_ = playback_name.isEmpty() ? QStringLiteral("未命名片段") : playback_name;
    playback_visible_count_ = 0;
    playback_step_ = qMax(1, (playback_points_.size() + 47) / 48);
    ecg_stream_.clear();
    emit playbackStateChanged();

    if (auto_start) {
        StartPlayback(true);
    }
}

void MonitorViewModel::UpdatePlaybackFrame() {
    if (playback_points_.isEmpty() || playback_visible_count_ <= 0) {
        ecg_stream_.clear();
        emit playbackStateChanged();
        return;
    }

    const int visible_count = qBound(0, playback_visible_count_, playback_points_.size());
    ecg_stream_.setPoints(playback_points_.mid(0, visible_count), playback_alert_level_);
    emit playbackStateChanged();
}

void MonitorViewModel::StartPlayback(bool restart_from_beginning) {
    if (playback_points_.isEmpty()) {
        return;
    }

    if (restart_from_beginning) {
        playback_visible_count_ = 0;
    }

    if (!is_playing_) {
        is_playing_ = true;
        emit playbackStateChanged();
    }

    if (playback_visible_count_ == 0) {
        playback_visible_count_ = qMin(playback_step_, playback_points_.size());
        UpdatePlaybackFrame();
    }

    if (playback_visible_count_ >= playback_points_.size()) {
        playback_timer_.stop();
        is_playing_ = false;
        emit playbackStateChanged();
        return;
    }

    playback_timer_.start();
}

void MonitorViewModel::UpdateEdgeState(ClientState state) {
    edge_state_ = EdgeStateName(state);
    if (state == ClientState::Connected) {
        edge_state_text_ = QStringLiteral("边缘推理已连接");
    } else if (state == ClientState::Connecting) {
        edge_state_text_ = QStringLiteral("边缘推理连接中");
    } else {
        edge_state_text_ = QStringLiteral("边缘推理未连接");
    }
    emit edgeStateChanged();
}

void MonitorViewModel::UpdateResult(const QString& pred_label,
                                    const QString& confidence,
                                    const QString& alert_level,
                                    const QString& latency_ms) {
    pred_label_ = pred_label;
    confidence_text_ = confidence;
    alert_level_ = alert_level;
    latency_text_ = latency_ms;
    confidence_percent_ = ConfidencePercent(confidence);
    emit resultChanged();
}

