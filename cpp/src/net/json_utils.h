#pragma once

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <sstream>
#include <string>
#include <utility>

namespace gp::json {

using boost::property_tree::ptree;

inline bool ParseObject(const std::string& payload, ptree* root, std::string* error_text) {
    if (root == nullptr) {
        if (error_text != nullptr) {
            *error_text = "内部错误：JSON 输出参数为空";
        }
        return false;
    }

    if (payload.empty()) {
        root->clear();
        return true;
    }

    try {
        std::istringstream iss(payload);
        boost::property_tree::read_json(iss, *root);
        return true;
    } catch (const std::exception& e) {
        if (error_text != nullptr) {
            *error_text = std::string("JSON 解析失败: ") + e.what();
        }
        return false;
    }
}

inline std::string Serialize(const ptree& root) {
    std::ostringstream oss;
    boost::property_tree::write_json(oss, root, false);
    std::string json = oss.str();
    while (!json.empty() && (json.back() == '\n' || json.back() == '\r')) {
        json.pop_back();
    }
    return json;
}

template <typename T>
inline void AppendArrayValue(ptree* array, const T& value) {
    ptree item;
    item.put("", value);
    array->push_back(std::make_pair("", item));
}

inline ptree MakeMessage(const std::string& message) {
    ptree root;
    root.put("message", message);
    return root;
}

}  // namespace gp::json
