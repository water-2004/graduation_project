#pragma once

#include "net/logic_handler.h"
#include "service/inference_engine.h"
#include "service/sample_repository.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace edge::logic {

// 路由/分发层：对外提供 JSON 协议包处理，对内保留命令式逻辑核心。
class LogicSystem : public edge::net::LogicHandler {
public:
    using CommandHandler = std::function<std::string(const std::string&, bool* close_conn)>;

    explicit LogicSystem(
        std::shared_ptr<service::InferenceEngine> engine,
        std::shared_ptr<service::SampleRepository> sample_repository = nullptr);

    gp::protocol::Packet HandlePacket(const gp::protocol::Packet& request, bool* close_conn) const override;
    std::string HandleLine(const std::string& line, bool* close_conn) const;

private:
    static std::string ToUpper(std::string text);
    static std::string Trim(const std::string& text);
    static std::string SerializeFeatureCsv(const std::vector<float>& features);
    static bool ParseFeatureCsv(
        const std::string& csv,
        std::size_t feature_dim,
        std::vector<float>* features,
        std::string* err);

    void Register(const std::string& cmd, CommandHandler handler);

    std::shared_ptr<service::InferenceEngine> engine_;
    std::shared_ptr<service::SampleRepository> sample_repository_;
    std::unordered_map<std::string, CommandHandler> handlers_;
};

}  // namespace edge::logic
