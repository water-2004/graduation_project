#pragma once

#include "logic/logic_system.h"

#include <boost/asio.hpp>

#include <memory>
#include <string>

namespace edge::net {

// 连接层：维护单个 TCP 连接的读写状态，调用逻辑层处理命令。
class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    TcpSession(boost::asio::io_context& io_context, std::shared_ptr<logic::LogicSystem> logic_system);

    boost::asio::ip::tcp::socket& socket() {
        return socket_;
    }

    void Start();

private:
    void DoRead();
    void DoWrite(const std::string& response, bool close_after_write);

    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf read_buffer_;
    std::shared_ptr<logic::LogicSystem> logic_system_;

    std::string write_buffer_;
    bool close_after_write_ = false;
};

}  // namespace edge::net
