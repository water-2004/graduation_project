#include "logic/logic_system.h"
#include "logger.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace edge::logic {

LogicSystem::LogicSystem(
    std::shared_ptr<service::InferenceEngine> engine,
    std::shared_ptr<service::SampleRepository> sample_repository)
    : engine_(std::move(engine)), sample_repository_(std::move(sample_repository)) {
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

    Register("LIST_SAMPLES", [this](const std::string&, bool*) {
        if (!sample_repository_) {
            return std::string("ERR 当前服务未配置样本目录");
        }

        const auto& names = sample_repository_->ListNames();
        std::ostringstream response;
        response << "SAMPLES";
        if (!names.empty()) {
            response << ' ';
            for (std::size_t i = 0; i < names.size(); ++i) {
                if (i > 0) {
                    response << ',';
                }
                response << names[i];
            }
        }

        gp::logging::Logger::Instance().Info("返回样本列表，数量: ", names.size());
        return response.str();
    });

    Register("PLAY_SAMPLE", [this](const std::string& payload, bool*) {
        if (!sample_repository_) {
            return std::string("ERR 当前服务未配置样本目录");
        }

        const std::string sample_name = Trim(payload);
        if (sample_name.empty()) {
            return std::string("ERR 样本名不能为空");
        }

        const service::BeatSample* sample = sample_repository_->FindByName(sample_name);
        if (sample == nullptr) {
            return std::string("ERR 样本不存在: ") + sample_name;
        }

        try {
            const service::PredictResult result = engine_->Predict(sample->values);
            std::ostringstream response;
            response << "BEAT name=" << sample->name
                     << " true=" << sample->true_label
                     << " pred=" << result.pred_label
                     << " conf=" << std::fixed << std::setprecision(6) << result.confidence
                     << " alert=" << result.alert_level
                     << " latency_ms=" << std::fixed << std::setprecision(3) << result.latency_ms
                     << " values=" << SerializeFeatureCsv(sample->values);

            gp::logging::Logger::Instance().Info(
                "播放样本完成 name=", sample->name,
                " true=", sample->true_label,
                " pred=", result.pred_label,
                " conf=", std::fixed, std::setprecision(6), result.confidence,
                " alert=", result.alert_level,
                " latency_ms=", std::fixed, std::setprecision(3), result.latency_ms);
            return response.str();
        } catch (const std::exception& e) {
            gp::logging::Logger::Instance().Error("播放样本推理失败: ", e.what());
            return std::string("ERR 播放样本失败: ") + e.what();
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
        return "ERR 未知命令，支持: PING | PREDICT ... | LIST_SAMPLES | PLAY_SAMPLE <name> | QUIT";
    }

    return it->second(payload, close_conn);
}

std::string LogicSystem::ToUpper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return text;
}

std::string LogicSystem::Trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::string LogicSystem::SerializeFeatureCsv(const std::vector<float>& features) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    for (std::size_t i = 0; i < features.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << features[i];
    }
    return oss.str();
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
