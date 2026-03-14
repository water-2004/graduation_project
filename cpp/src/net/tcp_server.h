#pragma once

#include "logic/logic_system.h"
#include "net/io_context_pool.h"

#include <boost/asio.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace edge::net {

// 接入层：只负责监听端口、接收连接，并把连接交给 TcpSession。
class TcpServer : public std::enable_shared_from_this<TcpServer> {
public:
    TcpServer(
        boost::asio::io_context& io_context,
        const std::string& host,
        std::uint16_t port,
        IOContextPool& io_pool,
        std::shared_ptr<logic::LogicSystem> logic_system);

    void Start();

private:
    void DoAccept();

    boost::asio::ip::tcp::acceptor acceptor_;
    IOContextPool& io_pool_;
    std::shared_ptr<logic::LogicSystem> logic_system_;
};

}  // namespace edge::net
