#include "net/tcp_server.h"

#include "logger.h"
#include "net/tcp_session.h"

#include <stdexcept>

namespace edge::net {

TcpServer::TcpServer(
    boost::asio::io_context& io_context,
    const std::string& host,
    std::uint16_t port,
    IOContextPool& io_pool,
    std::shared_ptr<logic::LogicSystem> logic_system)
    : acceptor_(io_context), io_pool_(io_pool), logic_system_(std::move(logic_system)) {
    const auto address = boost::asio::ip::make_address(host);
    boost::asio::ip::tcp::endpoint endpoint(address, port);

    boost::system::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        throw std::runtime_error("acceptor open failed: " + ec.message());
    }

    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), ec);
    if (ec) {
        throw std::runtime_error("set reuse_address failed: " + ec.message());
    }

    acceptor_.bind(endpoint, ec);
    if (ec) {
        throw std::runtime_error("acceptor bind failed: " + ec.message());
    }

    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        throw std::runtime_error("acceptor listen failed: " + ec.message());
    }
}

void TcpServer::Start() {
    DoAccept();
}

void TcpServer::DoAccept() {
    // 每个新会话从线程池中轮询分配一个 io_context。
    auto& session_io = io_pool_.GetIOContext();
    auto session = std::make_shared<TcpSession>(session_io, logic_system_);

    auto self = shared_from_this();
    acceptor_.async_accept(session->socket(), [self, session](const boost::system::error_code& ec) {
        if (ec) {
            gp::logging::Logger::Instance().Error("接入失败: ", ec.message());
        } else {
            try {
                const auto remote = session->socket().remote_endpoint();
                gp::logging::Logger::Instance().Info(
                    "客户端连接: ", remote.address().to_string(), ':', remote.port());
            } catch (const std::exception&) {
                gp::logging::Logger::Instance().Info("客户端连接建立");
            }
            session->Start();
        }

        // 无论成功与否都继续 accept，保持服务持续可用。
        self->DoAccept();
    });
}

}  // namespace edge::net
