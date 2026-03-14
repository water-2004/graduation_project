#include "inferpage.h"
#include "ui_inferpage.h"

#include <QDateTime>
#include <QLocale>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStringList>

#include <cmath>

#include "logger.h"
#include "tcpmgr.h"

namespace {

QString CurrentTimeText() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

}  // namespace

InferPage::InferPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::InferPage)
{
    ui->setupUi(this);
    bindSignals();
    resetResultFields();
    updateUiState(ClientState::Disconnected);

    ui->plainTextEditFeatures->setPlaceholderText(
        "请输入 187 维特征，使用英文逗号、空格或换行分隔。\n"
        "例如：0.12,0.15,-0.03,...");
    ui->plainTextEditLog->setMaximumBlockCount(500);
}

InferPage::~InferPage() {
    delete ui;
}

void InferPage::bindSignals() {
    connect(ui->pushButtonConnect, &QPushButton::clicked, this, &InferPage::onConnectClicked);
    connect(ui->pushButtonDisconnect, &QPushButton::clicked, this, &InferPage::onDisconnectClicked);
    connect(ui->pushButtonPing, &QPushButton::clicked, this, &InferPage::onPingClicked);
    connect(ui->pushButtonPredict, &QPushButton::clicked, this, &InferPage::onPredictClicked);
    connect(ui->pushButtonFillSample, &QPushButton::clicked, this, &InferPage::onFillSampleClicked);
    connect(ui->pushButtonClearLog, &QPushButton::clicked, this, &InferPage::onClearLogClicked);

    auto tcp_mgr = TcpMgr::GetInstance().get();
    connect(this, &InferPage::sig_tcp_connect, tcp_mgr, &TcpMgr::slot_tcp_connect);
    connect(this, &InferPage::sig_disconnect, tcp_mgr, &TcpMgr::slot_disconnect);
    connect(this, &InferPage::sig_send_ping, tcp_mgr, &TcpMgr::slot_send_ping);
    connect(this, &InferPage::sig_send_predict, tcp_mgr, &TcpMgr::slot_send_predict);

    connect(tcp_mgr, &TcpMgr::sig_con_success, this, &InferPage::onConnectResult);
    connect(tcp_mgr, &TcpMgr::sig_connection_closed, this, &InferPage::onConnectionClosed);
    connect(tcp_mgr, &TcpMgr::sig_state_changed, this, &InferPage::onStateChanged);
    connect(tcp_mgr, &TcpMgr::sig_log_message, this, &InferPage::appendLog);
    connect(tcp_mgr, &TcpMgr::sig_pong, this, &InferPage::onPong);
    connect(tcp_mgr, &TcpMgr::sig_predict_result, this, &InferPage::onPredictResult);
    connect(tcp_mgr, &TcpMgr::sig_server_error, this, &InferPage::onServerError);
    connect(tcp_mgr, &TcpMgr::sig_protocol_error, this, &InferPage::onProtocolError);
}

void InferPage::onConnectClicked() {
    const QString host = ui->lineEditHost->text().trimmed();
    if (host.isEmpty()) {
        gp::logging::Logger::Instance().WarningText("用户输入为空，连接被拒绝");
        QMessageBox::warning(this, "连接失败", "服务器 IP 不能为空。");
        return;
    }

    ServerInfo server_info;
    server_info.host = host;
    server_info.port = static_cast<quint16>(ui->spinBoxPort->value());
    emit sig_tcp_connect(server_info);
}

void InferPage::onDisconnectClicked() {
    emit sig_disconnect();
}

void InferPage::onPingClicked() {
    emit sig_send_ping();
}

void InferPage::onPredictClicked() {
    QString payload;
    QString error_message;
    if (!buildPredictPayload(&payload, &error_message)) {
        gp::logging::Logger::Instance().WarningText(QString("预测输入不合法: %1").arg(error_message).toUtf8().toStdString());
        QMessageBox::warning(this, "输入不合法", error_message);
        return;
    }

    emit sig_send_predict(payload);
}

void InferPage::onFillSampleClicked() {
    QStringList values;
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

        values.push_back(QString::number(value, 'f', 6));
    }

    ui->plainTextEditFeatures->setPlainText(values.join(","));
    appendLog("本地: 已填充一组 187 维演示特征");
}

void InferPage::onClearLogClicked() {
    ui->plainTextEditLog->clear();
}

void InferPage::onConnectResult(bool bsuccess) {
    if (!bsuccess) {
        gp::logging::Logger::Instance().WarningText("客户端连接失败");
        QMessageBox::warning(this, "连接失败", "无法连接服务器，请检查 IP、端口和服务是否启动。");
    }
}

void InferPage::onConnectionClosed() {
    resetResultFields();
    updateUiState(ClientState::Disconnected);
}

void InferPage::onStateChanged(ClientState state) {
    updateUiState(state);
}

void InferPage::onPong() {
    appendLog("本地: 心跳校验成功");
}

void InferPage::onPredictResult(const PredictResult& result) {
    ui->lineEditPred->setText(result.pred_label);
    ui->lineEditConf->setText(result.confidence);
    ui->lineEditAlert->setText(result.alert_level);
    ui->lineEditLatency->setText(result.latency_ms);
}

void InferPage::onServerError(const QString& text) {
    appendLog(QString("错误: %1").arg(text));
}

void InferPage::onProtocolError(const QString& text) {
    appendLog(QString("协议错误: %1").arg(text));
}

void InferPage::appendLog(const QString& text) {
    ui->plainTextEditLog->appendPlainText(QString("[%1] %2").arg(CurrentTimeText(), text));
}

void InferPage::updateUiState(ClientState state) {
    const bool is_connected = state == ClientState::Connected;
    const bool is_connecting = state == ClientState::Connecting;

    ui->pushButtonConnect->setEnabled(!is_connected && !is_connecting);
    ui->pushButtonDisconnect->setEnabled(is_connected || is_connecting);
    ui->pushButtonPing->setEnabled(is_connected);
    ui->pushButtonPredict->setEnabled(is_connected);
    ui->lineEditHost->setEnabled(!is_connected && !is_connecting);
    ui->spinBoxPort->setEnabled(!is_connected && !is_connecting);

    if (is_connected) {
        ui->labelConnectionStateValue->setText("已连接");
    } else if (is_connecting) {
        ui->labelConnectionStateValue->setText("连接中");
    } else {
        ui->labelConnectionStateValue->setText("未连接");
    }
}

bool InferPage::buildPredictPayload(QString* payload, QString* error_message) const {
    const QString text = ui->plainTextEditFeatures->toPlainText().trimmed();
    if (text.isEmpty()) {
        *error_message = "请先输入 187 维特征。";
        return false;
    }

    const QStringList tokens = text.split(QRegularExpression(R"([,\s]+)"), Qt::SkipEmptyParts);
    if (tokens.size() != 187) {
        *error_message = QString("特征数量不正确，当前为 %1，要求为 187。").arg(tokens.size());
        return false;
    }

    const QLocale locale = QLocale::c();
    QStringList normalized_values;
    normalized_values.reserve(tokens.size());

    for (int i = 0; i < tokens.size(); ++i) {
        bool ok = false;
        const double value = locale.toDouble(tokens.at(i), &ok);
        if (!ok) {
            *error_message = QString("第 %1 个特征不是合法数字。").arg(i + 1);
            return false;
        }
        normalized_values.push_back(QString::number(value, 'g', 10));
    }

    *payload = normalized_values.join(",");
    return true;
}

void InferPage::resetResultFields() {
    ui->lineEditPred->clear();
    ui->lineEditConf->clear();
    ui->lineEditAlert->clear();
    ui->lineEditLatency->clear();
}
