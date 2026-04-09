#pragma once

#include "protocol/message_protocol.h"

#include <functional>
#include <string>

namespace gp::backend {

gp::protocol::Packet HandleBusinessPacket(
    gp::protocol::MessageType message_type,
    const std::string& payload,
    const std::function<std::string(const std::string&, bool*)>& line_handler,
    bool* close_conn);

}  // namespace gp::backend
