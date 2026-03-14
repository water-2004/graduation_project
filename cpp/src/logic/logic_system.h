#pragma once

#include "service/inference_engine.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace edge::logic {

// 路由/分发层：负责命令注册与分发，不直接处理网络读写。
class LogicSystem {
public:
    using CommandHandler = std::function<std::string(const std::string&, bool* close_conn)>;

    explicit LogicSystem(std::shared_ptr<service::InferenceEngine> engine);

    // 处理一行请求并返回响应文本。
    std::string HandleLine(const std::string& line, bool* close_conn) const;

private:
    static std::string ToUpper(std::string text);
    static bool ParseFeatureCsv(
        const std::string& csv,
        std::size_t feature_dim,
        std::vector<float>* features,
        std::string* err);

    void Register(const std::string& cmd, CommandHandler handler);

    std::shared_ptr<service::InferenceEngine> engine_;
    std::unordered_map<std::string, CommandHandler> handlers_;
};

}  // namespace edge::logic
