#pragma once

#include "net/logic_handler.h"

#include <boost/asio.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace edge::net {

class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    TcpSession(boost::asio::io_context& io_context, std::shared_ptr<LogicHandler> logic_handler);

    boost::asio::ip::tcp::socket& socket() {
        return socket_;
    }

    void Start();

private:
    void DoReadHeader();
    void DoReadPayload(const gp::protocol::PacketHeader& header);
    void DoWrite(const gp::protocol::Packet& response, bool close_after_write);
    void CloseSocket();

    boost::asio::ip::tcp::socket socket_;
    std::shared_ptr<LogicHandler> logic_handler_;

    std::array<std::uint8_t, gp::protocol::kHeaderSize> read_header_buffer_{};
    std::vector<std::uint8_t> read_payload_buffer_;
    std::string write_buffer_;
    bool close_after_write_ = false;
};

}  // namespace edge::net
