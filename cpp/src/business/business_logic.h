#pragma once

#include "business/file_repository.h"
#include "net/logic_handler.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gp::backend {

// 业务逻辑层：负责登录、病人管理、监测记录和报警记录命令的解析与处理。
class BusinessLogic : public edge::net::LogicHandler {
public:
    using CommandHandler = std::function<std::string(const std::string&, bool* close_conn)>;

    explicit BusinessLogic(std::shared_ptr<FileRepository> repository);

    std::string HandleLine(const std::string& line, bool* close_conn) const override;

private:
    static std::string Trim(const std::string& text);
    static std::string ToUpper(std::string text);
    static std::vector<std::string> SplitByDelimiter(const std::string& text, char delimiter);
    static std::string FieldAt(const std::vector<std::string>& fields, std::size_t index);
    static std::string SerializePatient(const Patient& patient);
    static std::string SerializeMonitorRecord(const MonitorRecord& record);
    static std::string SerializeAlert(const AlertRecord& alert);

    void Register(const std::string& cmd, CommandHandler handler);

    std::shared_ptr<FileRepository> repository_;
    std::unordered_map<std::string, CommandHandler> handlers_;
};

}  // namespace gp::backend
