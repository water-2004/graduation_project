#ifndef GP_QML_MONITOR_VIEW_MODEL_H_
#define GP_QML_MONITOR_VIEW_MODEL_H_

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include "../Backend/NetworkService.h"
#include "../Models/EcgDataStream.h"
#include "../Models/HistoryModel.h"
#include "../global.h"

class MonitorViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString edgeState READ edgeState NOTIFY edgeStateChanged)
    Q_PROPERTY(QString edgeStateText READ edgeStateText NOTIFY edgeStateChanged)
    Q_PROPERTY(QString edgeHost READ edgeHost WRITE setEdgeHost NOTIFY edgeConfigChanged)
    Q_PROPERTY(int edgePort READ edgePort WRITE setEdgePort NOTIFY edgeConfigChanged)
    Q_PROPERTY(QString currentPatientId READ currentPatientId NOTIFY patientChanged)
    Q_PROPERTY(QString currentPatientName READ currentPatientName NOTIFY patientChanged)
    Q_PROPERTY(QString currentPatientSummary READ currentPatientSummary NOTIFY patientChanged)
    Q_PROPERTY(bool hasCurrentPatient READ hasCurrentPatient NOTIFY patientChanged)
    Q_PROPERTY(QStringList sampleNames READ sampleNames NOTIFY sampleNamesChanged)
    Q_PROPERTY(int selectedSampleIndex READ selectedSampleIndex WRITE setSelectedSampleIndex NOTIFY selectedSampleIndexChanged)
    Q_PROPERTY(QString predLabel READ predLabel NOTIFY resultChanged)
    Q_PROPERTY(QString confidenceText READ confidenceText NOTIFY resultChanged)
    Q_PROPERTY(QString alertLevel READ alertLevel NOTIFY resultChanged)
    Q_PROPERTY(QString latencyText READ latencyText NOTIFY resultChanged)
    Q_PROPERTY(int confidencePercent READ confidencePercent NOTIFY resultChanged)
    Q_PROPERTY(EcgDataStream* ecgStream READ ecgStream CONSTANT)
    Q_PROPERTY(HistoryModel* historyModel READ historyModel CONSTANT)
    Q_PROPERTY(bool playbackAvailable READ playbackAvailable NOTIFY playbackStateChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playbackStateChanged)
    Q_PROPERTY(double playbackProgress READ playbackProgress NOTIFY playbackStateChanged)
    Q_PROPERTY(QString playbackProgressText READ playbackProgressText NOTIFY playbackStateChanged)
    Q_PROPERTY(QString currentPlaybackName READ currentPlaybackName NOTIFY playbackStateChanged)

public:
    explicit MonitorViewModel(NetworkService* network_service, QObject* parent = nullptr);

    QString edgeState() const;
    QString edgeStateText() const;
    QString edgeHost() const;
    void setEdgeHost(const QString& host);
    int edgePort() const;
    void setEdgePort(int port);

    QString currentPatientId() const;
    QString currentPatientName() const;
    QString currentPatientSummary() const;
    bool hasCurrentPatient() const;

    QStringList sampleNames() const;
    int selectedSampleIndex() const;
    void setSelectedSampleIndex(int index);

    QString predLabel() const;
    QString confidenceText() const;
    QString alertLevel() const;
    QString latencyText() const;
    int confidencePercent() const;
    EcgDataStream* ecgStream();
    HistoryModel* historyModel();
    bool playbackAvailable() const;
    bool isPlaying() const;
    double playbackProgress() const;
    QString playbackProgressText() const;
    QString currentPlaybackName() const;

    void setCurrentPatient(const PatientInfo& patient);
    void clearCurrentPatient();

    Q_INVOKABLE void connectEdge();
    Q_INVOKABLE void disconnectEdge();
    Q_INVOKABLE void sendPing();
    Q_INVOKABLE void loadSamples();
    Q_INVOKABLE void playSelectedSample();
    Q_INVOKABLE QString generateDemoInput() const;
    Q_INVOKABLE void sendPredict(const QString& feature_text);
    Q_INVOKABLE void replayHistory(int index);
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void seekPlayback(double progress);

signals:
    void edgeStateChanged();
    void edgeConfigChanged();
    void patientChanged();
    void sampleNamesChanged();
    void selectedSampleIndexChanged();
    void resultChanged();
    void playbackStateChanged();
    void toastRequested(const QString& message, const QString& level);
    void monitorRecordReady(const MonitorRecordInfo& record_info);

private:
    static QString BuildCsvText(const QVector<double>& values);
    static bool ParseFeatureValues(const QString& text, QVector<double>* values, QString* error_text);
    static QVariantList ToVariantList(const QVector<double>& values);
    static int ConfidencePercent(const QString& text);

    void OnPredictResult(const PredictResult& result,
                         const QVector<double>& values,
                         const QString& source,
                         const QString& sample_name,
                         const QString& true_label);
    void SetPlaybackSource(const QVariantList& points,
                           const QString& alert_level,
                           const QString& playback_name,
                           bool auto_start);
    void UpdatePlaybackFrame();
    void StartPlayback(bool restart_from_beginning);
    void UpdateEdgeState(ClientState state);
    void UpdateResult(const QString& pred_label,
                      const QString& confidence,
                      const QString& alert_level,
                      const QString& latency_ms);

    NetworkService* network_service_ = nullptr;
    EcgDataStream ecg_stream_;
    HistoryModel history_model_;
    QString edge_state_ = QStringLiteral("disconnected");
    QString edge_state_text_ = QStringLiteral("边缘服务未连接");
    QString edge_host_ = QStringLiteral("127.0.0.1");
    int edge_port_ = 9000;
    QStringList sample_names_;
    int selected_sample_index_ = -1;
    PatientInfo current_patient_;
    bool has_current_patient_ = false;
    QString pred_label_;
    QString confidence_text_;
    QString alert_level_ = QStringLiteral("normal");
    QString latency_text_;
    int confidence_percent_ = 0;
    QVector<double> manual_values_;
    QTimer playback_timer_;
    QVariantList playback_points_;
    QString playback_alert_level_ = QStringLiteral("normal");
    QString current_playback_name_;
    int playback_visible_count_ = 0;
    int playback_step_ = 1;
    bool is_playing_ = false;
    bool auto_load_samples_after_connect_ = false;

};

#endif  // GP_QML_MONITOR_VIEW_MODEL_H_
