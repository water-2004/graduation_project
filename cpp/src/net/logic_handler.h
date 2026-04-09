#pragma once

#include "protocol/message_protocol.h"

namespace edge::net {

// 网络层抽象接口：接收一个完整协议包，返回一个完整响应包。
class LogicHandler {
public:
    virtual ~LogicHandler() = default;

    virtual gp::protocol::Packet HandlePacket(
        const gp::protocol::Packet& request,
        bool* close_conn) const = 0;
};

}  // namespace edge::net
