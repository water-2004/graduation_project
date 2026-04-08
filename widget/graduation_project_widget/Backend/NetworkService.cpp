#include "NetworkService.h"

NetworkService::NetworkService(QObject* parent)
    : QObject(parent)
    , tcp_mgr_(TcpMgr::GetInstance().get()) {
    connect(tcp_mgr_, &TcpMgr::sig_state_changed, this, &NetworkService::stateChanged);
    connect(tcp_mgr_, &TcpMgr::sig_log_message, this, &NetworkService::logMessage);
    connect(tcp_mgr_, &TcpMgr::sig_connection_closed, this, &NetworkService::connectionClosed);
    connect(tcp_mgr_, &TcpMgr::sig_con_success, this, &NetworkService::connectResult);
    connect(tcp_mgr_, &TcpMgr::sig_pong, this, &NetworkService::pong);
    connect(tcp_mgr_, &TcpMgr::sig_predict_result, this, &NetworkService::predictResult);
    connect(tcp_mgr_, &TcpMgr::sig_sample_list_ready, this, &NetworkService::sampleListReady);
    connect(tcp_mgr_, &TcpMgr::sig_beat_response, this, &NetworkService::beatResponse);
    connect(tcp_mgr_, &TcpMgr::sig_server_error, this, &NetworkService::serverError);
    connect(tcp_mgr_, &TcpMgr::sig_protocol_error, this, &NetworkService::protocolError);
}

ClientState NetworkService::state() const {
    return tcp_mgr_->state();
}

void NetworkService::connectEdge(const QString& host, int port) {
    ServerInfo info;
    info.host = host;
    info.port = static_cast<quint16>(port);
    tcp_mgr_->slot_tcp_connect(info);
}

void NetworkService::disconnectEdge() { tcp_mgr_->slot_disconnect(); }
void NetworkService::sendPing() { tcp_mgr_->slot_send_ping(); }
void NetworkService::sendPredict(const QString& payload) { tcp_mgr_->slot_send_predict(payload); }
void NetworkService::loadSamples() { tcp_mgr_->slot_list_samples(); }
void NetworkService::playSample(const QString& sample_name) { tcp_mgr_->slot_play_sample(sample_name); }
