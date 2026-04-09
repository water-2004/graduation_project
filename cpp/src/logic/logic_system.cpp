#include "logic/logic_system.h"

#include "logger.h"
#include "net/json_utils.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using gp::json::ptree;

bool StartsWith(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

gp::protocol::Packet BuildErrorPacket(const std::string& code, const std::string& message) {
    ptree root;
    root.put("code", code);
    root.put("message", message);
    return {gp::protocol::MessageType::kErrorResponse, gp::json::Serialize(root)};
}

gp::protocol::Packet BuildPacket(gp::protocol::MessageType type, const ptree& root) {
    return {type, gp::json::Serialize(root)};
}

bool ReadStringField(const ptree& root, const std::string& key, std::string* value, std::string* error_text) {
    const auto field = root.get_optional<std::string>(key);
    if (!field.has_value()) {
        if (error_text != nullptr) {
            *error_text = "缺少字段: " + key;
        }
        return false;
    }
    *value = field.value();
    return true;
}

bool ReadFloatArrayField(
    const ptree& root,
    const std::string& key,
    std::vector<float>* values,
    std::string* error_text) {
    if (values == nullptr) {
        if (error_text != nullptr) {
            *error_text = "内部错误：输出特征数组参数为空";
        }
        return false;
    }

    const auto child = root.get_child_optional(key);
    if (!child.has_value()) {
        if (error_text != nullptr) {
            *error_text = "缺少数组字段: " + key;
        }
        return false;
    }

    values->clear();
    for (const auto& item : child.value()) {
        try {
            values->push_back(item.second.get_value<float>());
        } catch (const std::exception&) {
            if (error_text != nullptr) {
                *error_text = "数组字段不是合法浮点数: " + key;
            }
            return false;
        }
    }
    return true;
}

bool ParseFeatureCsvLocal(
    const std::string& csv,
    std::size_t feature_dim,
    std::vector<float>* features,
    std::string* error_text) {
    if (features == nullptr) {
        if (error_text != nullptr) {
            *error_text = "内部错误：输出特征参数为空";
        }
        return false;
    }

    std::vector<float> parsed;
    parsed.reserve(feature_dim == 0 ? 8 : feature_dim);

    std::stringstream stream(csv);
    std::string token;
    while (std::getline(stream, token, ',')) {
        if (token.empty()) {
            if (error_text != nullptr) {
                *error_text = "存在空特征值";
            }
            return false;
        }

        try {
            parsed.push_back(std::stof(token));
        } catch (const std::exception&) {
            if (error_text != nullptr) {
                *error_text = "特征值不是合法数字";
            }
            return false;
        }
    }

    if (feature_dim != 0 && parsed.size() != feature_dim) {
        if (error_text != nullptr) {
            std::ostringstream oss;
            oss << "特征数量错误，期望 " << feature_dim << "，实际 " << parsed.size();
            *error_text = oss.str();
        }
        return false;
    }

    *features = std::move(parsed);
    return true;
}

bool ParsePredictResponse(
    const std::string& line,
    std::string* pred_label,
    double* confidence,
    std::string* alert_level,
    double* latency_ms) {
    static const std::regex kResponseRe(
        R"(^OK\s+pred=(\S+)\s+conf=([-\d\.eE\+]+)\s+alert=(\S+)\s+latency_ms=([-\d\.eE\+]+)$)");
    std::smatch match;
    if (!std::regex_match(line, match, kResponseRe)) {
        return false;
    }

    *pred_label = match[1].str();
    *confidence = std::stod(match[2].str());
    *alert_level = match[3].str();
    *latency_ms = std::stod(match[4].str());
    return true;
}

bool ParseBeatResponse(
    const std::string& line,
    std::string* sample_name,
    std::string* true_label,
    std::string* pred_label,
    double* confidence,
    std::string* alert_level,
    double* latency_ms,
    std::vector<float>* values) {
    static const std::regex kResponseRe(
        R"(^BEAT\s+name=(\S+)\s+true=(\S+)\s+pred=(\S+)\s+conf=([-\d\.eE\+]+)\s+alert=(\S+)\s+latency_ms=([-\d\.eE\+]+)\s+values=(.+)$)");
    std::smatch match;
    if (!std::regex_match(line, match, kResponseRe)) {
        return false;
    }

    *sample_name = match[1].str();
    *true_label = match[2].str();
    *pred_label = match[3].str();
    *confidence = std::stod(match[4].str());
    *alert_level = match[5].str();
    *latency_ms = std::stod(match[6].str());

    std::string error_text;
    const std::size_t feature_dim = match[7].str().empty()
        ? 0
        : static_cast<std::size_t>(std::count(match[7].first, match[7].second, ',') + 1);
    return ParseFeatureCsvLocal(match[7].str(), feature_dim, values, &error_text);
}

std::vector<std::string> SplitCommaValues(const std::string& text) {
    std::vector<std::string> result;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

}  // namespace

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

gp::protocol::Packet LogicSystem::HandlePacket(const gp::protocol::Packet& request, bool* close_conn) const {
    if (close_conn != nullptr) {
        *close_conn = false;
    }

    switch (request.message_type) {
    case gp::protocol::MessageType::kPingRequest: {
        bool local_close = false;
        const std::string response_line = HandleLine("PING", &local_close);
        if (close_conn != nullptr) {
            *close_conn = local_close;
        }
        ptree root;
        root.put("message", response_line);
        return BuildPacket(gp::protocol::MessageType::kPingResponse, root);
    }
    case gp::protocol::MessageType::kQuitRequest: {
        bool local_close = false;
        const std::string response_line = HandleLine("QUIT", &local_close);
        if (close_conn != nullptr) {
            *close_conn = local_close;
        }
        ptree root;
        root.put("message", response_line);
        return BuildPacket(gp::protocol::MessageType::kByeResponse, root);
    }
    case gp::protocol::MessageType::kPredictRequest: {
        ptree root;
        std::string error_text;
        if (!gp::json::ParseObject(request.payload, &root, &error_text)) {
            return BuildErrorPacket("invalid_json", error_text);
        }

        std::vector<float> features;
        if (!ReadFloatArrayField(root, "features", &features, &error_text)) {
            return BuildErrorPacket("bad_request", error_text);
        }

        const std::string response_line = HandleLine("PREDICT " + SerializeFeatureCsv(features), close_conn);
        if (StartsWith(response_line, "ERR ")) {
            return BuildErrorPacket("predict_failed", Trim(response_line.substr(4)));
        }

        std::string pred_label;
        std::string alert_level;
        double confidence = 0.0;
        double latency_ms = 0.0;
        if (!ParsePredictResponse(response_line, &pred_label, &confidence, &alert_level, &latency_ms)) {
            return BuildErrorPacket("invalid_response", "推理响应格式无法解析");
        }

        ptree response_root;
        response_root.put("pred_label", pred_label);
        response_root.put("confidence", confidence);
        response_root.put("alert_level", alert_level);
        response_root.put("latency_ms", latency_ms);
        return BuildPacket(gp::protocol::MessageType::kPredictResponse, response_root);
    }
    case gp::protocol::MessageType::kListSamplesRequest: {
        const std::string response_line = HandleLine("LIST_SAMPLES", close_conn);
        if (StartsWith(response_line, "ERR ")) {
            return BuildErrorPacket("list_samples_failed", Trim(response_line.substr(4)));
        }
        if (!StartsWith(response_line, "SAMPLES")) {
            return BuildErrorPacket("invalid_response", "样本列表响应格式无法解析");
        }

        ptree response_root;
        ptree samples;
        const std::string payload = Trim(response_line.substr(std::string("SAMPLES").size()));
        for (const auto& name : SplitCommaValues(payload)) {
            gp::json::AppendArrayValue(&samples, name);
        }
        response_root.add_child("samples", samples);
        return BuildPacket(gp::protocol::MessageType::kListSamplesResponse, response_root);
    }
    case gp::protocol::MessageType::kPlaySampleRequest: {
        ptree root;
        std::string error_text;
        if (!gp::json::ParseObject(request.payload, &root, &error_text)) {
            return BuildErrorPacket("invalid_json", error_text);
        }

        std::string sample_name;
        if (!ReadStringField(root, "sample_name", &sample_name, &error_text)) {
            return BuildErrorPacket("bad_request", error_text);
        }

        const std::string response_line = HandleLine("PLAY_SAMPLE " + sample_name, close_conn);
        if (StartsWith(response_line, "ERR ")) {
            return BuildErrorPacket("play_sample_failed", Trim(response_line.substr(4)));
        }

        std::string parsed_sample_name;
        std::string true_label;
        std::string pred_label;
        std::string alert_level;
        double confidence = 0.0;
        double latency_ms = 0.0;
        std::vector<float> values;
        if (!ParseBeatResponse(
                response_line,
                &parsed_sample_name,
                &true_label,
                &pred_label,
                &confidence,
                &alert_level,
                &latency_ms,
                &values)) {
            return BuildErrorPacket("invalid_response", "播放样本响应格式无法解析");
        }

        ptree response_root;
        response_root.put("sample_name", parsed_sample_name);
        response_root.put("true_label", true_label);
        response_root.put("pred_label", pred_label);
        response_root.put("confidence", confidence);
        response_root.put("alert_level", alert_level);
        response_root.put("latency_ms", latency_ms);

        ptree values_array;
        for (float value : values) {
            gp::json::AppendArrayValue(&values_array, value);
        }
        response_root.add_child("values", values_array);
        return BuildPacket(gp::protocol::MessageType::kPlaySampleResponse, response_root);
    }
    default:
        return BuildErrorPacket(
            "unsupported_type",
            std::string("推理服务不支持的消息类型: ") + gp::protocol::MessageTypeName(request.message_type));
    }
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
    return ParseFeatureCsvLocal(csv, feature_dim, features, err);
}

void LogicSystem::Register(const std::string& cmd, CommandHandler handler) {
    handlers_[cmd] = std::move(handler);
}

}  // namespace edge::logic
