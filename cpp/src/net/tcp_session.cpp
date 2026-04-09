#include "net/tcp_session.h"

#include "logger.h"
#include "net/json_utils.h"

namespace {

gp::protocol::Packet BuildErrorPacket(const std::string& code, const std::string& message) {
    gp::json::ptree root;
    root.put("code", code);
    root.put("message", message);
    return {gp::protocol::MessageType::kErrorResponse, gp::json::Serialize(root)};
}

}  // namespace

namespace edge::net {

TcpSession::TcpSession(boost::asio::io_context& io_context, std::shared_ptr<LogicHandler> logic_handler)
    : socket_(io_context), logic_handler_(std::move(logic_handler)) {}

void TcpSession::Start() {
    DoReadHeader();
}

void TcpSession::DoReadHeader() {
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(read_header_buffer_),
        [self](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                if (ec != boost::asio::error::eof && ec != boost::asio::error::connection_reset) {
                    gp::logging::Logger::Instance().Error("会话读取包头失败: ", ec.message());
                }
                return;
            }

            gp::protocol::PacketHeader header;
            std::string error_text;
            if (!gp::protocol::DecodeHeader(
                    self->read_header_buffer_.data(),
                    self->read_header_buffer_.size(),
                    &header,
                    &error_text)) {
                gp::logging::Logger::Instance().Warning("协议包头校验失败: ", error_text);
                self->DoWrite(BuildErrorPacket("invalid_header", error_text), true);
                return;
            }

            self->DoReadPayload(header);
        });
}

void TcpSession::DoReadPayload(const gp::protocol::PacketHeader& header) {
    read_payload_buffer_.assign(header.payload_length, 0);
    if (header.payload_length == 0) {
        bool close_conn = false;
        const gp::protocol::Packet request{header.message_type, std::string()};
        const gp::protocol::Packet response = logic_handler_->HandlePacket(request, &close_conn);
        DoWrite(response, close_conn);
        return;
    }

    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(read_payload_buffer_),
        [self, header](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                gp::logging::Logger::Instance().Error("会话读取包体失败: ", ec.message());
                return;
            }

            gp::protocol::Packet request;
            request.message_type = header.message_type;
            request.payload.assign(
                reinterpret_cast<const char*>(self->read_payload_buffer_.data()),
                self->read_payload_buffer_.size());

            bool close_conn = false;
            const gp::protocol::Packet response = self->logic_handler_->HandlePacket(request, &close_conn);
            self->DoWrite(response, close_conn);
        });
}

void TcpSession::DoWrite(const gp::protocol::Packet& response, bool close_after_write) {
    write_buffer_ = gp::protocol::EncodePacket(response);
    close_after_write_ = close_after_write;

    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_buffer_),
        [self](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                gp::logging::Logger::Instance().Error("会话写入失败: ", ec.message());
                self->CloseSocket();
                return;
            }

            if (self->close_after_write_) {
                self->CloseSocket();
                gp::logging::Logger::Instance().Info("会话已按请求关闭");
                return;
            }

            self->DoReadHeader();
        });
}

void TcpSession::CloseSocket() {
    boost::system::error_code ignore;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore);
    socket_.close(ignore);
}

}  // namespace edge::net
