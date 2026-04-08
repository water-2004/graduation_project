#ifndef GP_QML_NETWORK_SERVICE_H_
#define GP_QML_NETWORK_SERVICE_H_

#include <QObject>

#include "../tcpmgr.h"

class NetworkService : public QObject {
    Q_OBJECT

public:
    explicit NetworkService(QObject* parent = nullptr);

    ClientState state() const;

public slots:
    void connectEdge(const QString& host, int port);
    void disconnectEdge();
    void sendPing();
    void sendPredict(const QString& payload);
    void loadSamples();
    void playSample(const QString& sample_name);

signals:
    void stateChanged(ClientState state);
    void logMessage(const QString& text);
    void connectionClosed();
    void connectResult(bool success);
    void pong();
    void predictResult(const PredictResult& result);
    void sampleListReady(const QStringList& samples);
    void beatResponse(const BeatResponse& response);
    void serverError(const QString& text);
    void protocolError(const QString& text);

private:
    TcpMgr* tcp_mgr_ = nullptr;
};

#endif  // GP_QML_NETWORK_SERVICE_H_
