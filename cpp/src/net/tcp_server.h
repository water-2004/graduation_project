#pragma once

#include "net/logic_handler.h"
#include "net/io_context_pool.h"

#include <boost/asio.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace edge::net {

class TcpServer : public std::enable_shared_from_this<TcpServer> {
public:
    TcpServer(
        boost::asio::io_context& io_context,
        const std::string& host,
        std::uint16_t port,
        IOContextPool& io_pool,
        std::shared_ptr<LogicHandler> logic_handler);

    void Start();

private:
    void DoAccept();

    boost::asio::ip::tcp::acceptor acceptor_;
    IOContextPool& io_pool_;
    std::shared_ptr<LogicHandler> logic_handler_;
};

}  // namespace edge::net
