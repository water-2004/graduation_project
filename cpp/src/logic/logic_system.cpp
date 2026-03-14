#include "logic/logic_system.h"
#include "logger.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace edge::logic {

LogicSystem::LogicSystem(std::shared_ptr<service::InferenceEngine> engine)
    : engine_(std::move(engine)) {
    Register("PING", [](const std::string&, bool*) {
        return std::string("PONG");
    });

    Register("QUIT", [](const std::string&, bool* close_conn) {
        if (close_conn != nullptr) {
            *close_conn = true;
        }
        return std::string("BYE");
    });

    Register("PREDICT", [this](const std::string& payload, bool*) {
        std::vector<float> features;
        std::string err;
        if (!ParseFeatureCsv(payload, engine_->feature_dim(), &features, &err)) {
            gp::logging::Logger::Instance().Warning("PREDICT 请求解析失败: ", err);
            return std::string("ERR ") + err;
        }

        try {
            const service::PredictResult result = engine_->Predict(features);
            std::ostringstream response;
            response << "OK pred=" << result.pred_label
                     << " conf=" << std::fixed << std::setprecision(6) << result.confidence
                     << " alert=" << result.alert_level
                     << " latency_ms=" << std::fixed << std::setprecision(3) << result.latency_ms;

            gp::logging::Logger::Instance().Info(
                "推理完成 pred=", result.pred_label,
                " conf=", std::fixed, std::setprecision(6), result.confidence,
                " alert=", result.alert_level,
                " latency_ms=", std::fixed, std::setprecision(3), result.latency_ms);
            return response.str();
        } catch (const std::exception& e) {
            gp::logging::Logger::Instance().Error("推理失败: ", e.what());
            return std::string("ERR 推理失败: ") + e.what();
        }
    });
}

std::string LogicSystem::HandleLine(const std::string& line, bool* close_conn) const {
    if (close_conn != nullptr) {
        *close_conn = false;
    }

    std::string trimmed = line;
    while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == '\n')) {
        trimmed.pop_back();
    }
    if (trimmed.empty()) {
        gp::logging::Logger::Instance().Warning("收到空命令");
        return "ERR 空命令";
    }

    std::string cmd;
    std::string payload;
    const auto pos = trimmed.find(' ');
    if (pos == std::string::npos) {
        cmd = ToUpper(trimmed);
    } else {
        cmd = ToUpper(trimmed.substr(0, pos));
        payload = trimmed.substr(pos + 1);
    }

    gp::logging::Logger::Instance().Debug("收到命令: ", cmd);

    const auto it = handlers_.find(cmd);
    if (it == handlers_.end()) {
        gp::logging::Logger::Instance().Warning("未知命令: ", cmd);
        return "ERR 未知命令，支持: PING | PREDICT ... | QUIT";
    }

    return it->second(payload, close_conn);
}

std::string LogicSystem::ToUpper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return text;
}

bool LogicSystem::ParseFeatureCsv(
    const std::string& csv,
    std::size_t feature_dim,
    std::vector<float>* features,
    std::string* err) {
    std::vector<float> parsed;
    parsed.reserve(feature_dim);

    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (token.empty()) {
            if (err != nullptr) {
                *err = "存在空特征值";
            }
            return false;
        }

        try {
            parsed.push_back(std::stof(token));
        } catch (const std::exception&) {
            if (err != nullptr) {
                *err = "特征值不是合法数字";
            }
            return false;
        }
    }

    if (parsed.size() != feature_dim) {
        if (err != nullptr) {
            std::ostringstream oss;
            oss << "特征数量错误，期望 " << feature_dim << "，实际 " << parsed.size();
            *err = oss.str();
        }
        return false;
    }

    *features = std::move(parsed);
    return true;
}

void LogicSystem::Register(const std::string& cmd, CommandHandler handler) {
    handlers_[cmd] = std::move(handler);
}

}  // namespace edge::logic
