#ifndef INFERPAGE_H
#define INFERPAGE_H

#include <QWidget>

#include "global.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class InferPage;
}
QT_END_NAMESPACE

// 页面层：负责用户交互和结果展示，不直接操作 QTcpSocket。
class InferPage : public QWidget
{
    Q_OBJECT

public:
    explicit InferPage(QWidget *parent = nullptr);
    ~InferPage();

signals:
    void sig_tcp_connect(ServerInfo server_info);
    void sig_disconnect();
    void sig_send_ping();
    void sig_send_predict(const QString& payload);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onPingClicked();
    void onPredictClicked();
    void onFillSampleClicked();
    void onClearLogClicked();

    void onConnectResult(bool bsuccess);
    void onConnectionClosed();
    void onStateChanged(ClientState state);
    void onPong();
    void onPredictResult(const PredictResult& result);
    void onServerError(const QString& text);
    void onProtocolError(const QString& text);
    void appendLog(const QString& text);

private:
    void bindSignals();
    void updateUiState(ClientState state);
    bool buildPredictPayload(QString* payload, QString* error_message) const;
    void resetResultFields();

    Ui::InferPage *ui;
};

#endif // INFERPAGE_H
