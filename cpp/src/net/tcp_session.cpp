#include "net/tcp_session.h"

#include "logger.h"

namespace edge::net {

TcpSession::TcpSession(boost::asio::io_context& io_context, std::shared_ptr<logic::LogicSystem> logic_system)
    : socket_(io_context), logic_system_(std::move(logic_system)) {}

void TcpSession::Start() {
    DoRead();
}

void TcpSession::DoRead() {
    auto self = shared_from_this();
    // 按“行”读取协议消息（\n 分隔）。
    boost::asio::async_read_until(socket_, read_buffer_, '\n', [self](const boost::system::error_code& ec, std::size_t) {
        if (ec) {
            if (ec != boost::asio::error::eof && ec != boost::asio::error::connection_reset) {
                gp::logging::Logger::Instance().Error("会话读取失败: ", ec.message());
            }
            return;
        }

        std::istream stream(&self->read_buffer_);
        std::string line;
        std::getline(stream, line);

        bool close_conn = false;
        const std::string response = self->logic_system_->HandleLine(line, &close_conn);
        self->DoWrite(response, close_conn);
    });
}

void TcpSession::DoWrite(const std::string& response, bool close_after_write) {
    write_buffer_ = response + "\n";
    close_after_write_ = close_after_write;

    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(write_buffer_), [self](const boost::system::error_code& ec, std::size_t) {
        if (ec) {
            gp::logging::Logger::Instance().Error("会话写入失败: ", ec.message());
            return;
        }

        if (self->close_after_write_) {
            // QUIT 等命令需要回写后主动断开。
            boost::system::error_code ignore;
            self->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore);
            self->socket_.close(ignore);
            gp::logging::Logger::Instance().Info("会话已按请求关闭");
            return;
        }

        // 保持长连接，继续读取下一条命令。
        self->DoRead();
    });
}

}  // namespace edge::net
