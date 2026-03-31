#pragma once

#include "net/logic_handler.h"

#include <boost/asio.hpp>

#include <memory>
#include <string>

namespace edge::net {

constexpr std::size_t kMaxLineLength = 64 * 1024;  // 64 KB

class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    TcpSession(boost::asio::io_context& io_context, std::shared_ptr<LogicHandler> logic_handler);

    boost::asio::ip::tcp::socket& socket() {
        return socket_;
    }

    void Start();

private:
    void DoRead();
    void DoWrite(const std::string& response, bool close_after_write);

    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf read_buffer_;
    std::shared_ptr<LogicHandler> logic_handler_;

    std::string write_buffer_;
    bool close_after_write_ = false;
};

}  // namespace edge::net
