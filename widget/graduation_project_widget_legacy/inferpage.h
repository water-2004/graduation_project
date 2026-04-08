#ifndef INFERPAGE_H
#define INFERPAGE_H

#include <QVector>
#include <QStringList>
#include <QWidget>

#include "global.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class InferPage;
}
QT_END_NAMESPACE

class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QProgressBar;
class QTableWidget;
class EcgWaveWidget;

struct BeatHistoryEntry {
    QString time;
    QString pred_label;
    QString confidence;
    QString alert_level;
    QString latency_ms;
    QString source;
    QVector<double> values;
};

// 页面层：负责用户交互和结果展示，不直接操作 QTcpSocket。
class InferPage : public QWidget {
    Q_OBJECT

public:
    explicit InferPage(QWidget* parent = nullptr);
    ~InferPage();

    void SetCurrentPatient(const PatientInfo& patient_info);
    void ClearCurrentPatient();
    bool HasCurrentPatient() const;
    QString CurrentPatientId() const;

signals:
    void sig_tcp_connect(ServerInfo server_info);
    void sig_disconnect();
    void sig_send_ping();
    void sig_send_predict(const QString& payload);
    void sig_list_samples();
    void sig_play_sample(const QString& sample_name);
    void sig_back_to_patients();
    void sig_add_monitor_record(const MonitorRecordInfo& record_info);

public slots:
    void slot_append_log(const QString& text);
    void slot_on_monitor_record_saved(const QString& message);
    void slot_on_business_error(const QString& error_text);
    void slot_on_business_connection_closed();

private slots:
    void OnConnectClicked();
    void OnDisconnectClicked();
    void OnPingClicked();
    void OnPredictClicked();
    void OnFillSampleClicked();
    void OnLoadSamplesClicked();
    void OnPlaySampleClicked();
    void OnClearLogClicked();
    void OnBackClicked();

    void OnConnectResult(bool bsuccess);
    void OnConnectionClosed();
    void OnStateChanged(ClientState state);
    void OnPong();
    void OnPredictResult(const PredictResult& result);
    void OnSampleListReceived(const QStringList& samples);
    void OnBeatResponse(const BeatResponse& response);
    void OnServerError(const QString& text);
    void OnProtocolError(const QString& text);
    void AppendLog(const QString& text);

private:
    void BindSignals();
    void CreateDynamicPanels();
    void UpdateUiState(ClientState state);
    void UpdatePatientBanner();
    void SubmitMonitorRecord(const QString& pred_label,
                             const QString& confidence,
                             const QString& alert_level,
                             const QString& latency_ms,
                             const QString& source,
                             const QString& sample_name,
                             const QString& true_label);
    void NotifyAlarmIfNeeded(const QString& alert_level, const QString& pred_label);
    bool ParseFeatureValues(QVector<double>* values, QString* error_message) const;
    static QString BuildCsvText(const QVector<double>& values);
    void UpdateResultFields(const QString& pred,
                            const QString& confidence,
                            const QString& alert_level,
                            const QString& latency_ms);
    void ApplyAlertStyle(const QString& alert_level);
    void ResetResultFields();
    void AddHistoryEntry(const QString& pred_label, const QString& confidence,
                         const QString& alert_level, const QString& latency_ms,
                         const QString& source, const QVector<double>& values);
    void OnHistoryRowClicked(int row);

    static constexpr int kMaxHistoryEntries = 20;

    Ui::InferPage* ui;
    QGroupBox* groupBoxSamples_ = nullptr;
    QComboBox* comboBoxSamples_ = nullptr;
    QPushButton* pushButtonLoadSamples_ = nullptr;
    QPushButton* pushButtonPlaySample_ = nullptr;
    QPushButton* pushButtonBack_ = nullptr;
    QLabel* labelCurrentPatient_ = nullptr;
    QProgressBar* confidence_progress_bar_ = nullptr;
    EcgWaveWidget* waveWidget_ = nullptr;
    QTableWidget* historyTable_ = nullptr;
    QVector<BeatHistoryEntry> history_entries_;
    QVector<double> manual_values_;
    PatientInfo current_patient_;
    bool has_current_patient_ = false;
};

#endif // INFERPAGE_H

