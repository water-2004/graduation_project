#pragma once

#include <string>

namespace edge::net {

// Abstract interface for command-line protocol handlers.
// Both LogicSystem (inference) and BusinessLogic (business) implement this.
class LogicHandler {
public:
    virtual ~LogicHandler() = default;
    virtual std::string HandleLine(const std::string& line, bool* close_conn) const = 0;
};

}  // namespace edge::net
