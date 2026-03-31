#include "service/sample_repository.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace edge::service {
namespace {

std::vector<std::string> Tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;

    for (unsigned char ch : text) {
        if (ch == ',' || std::isspace(ch) != 0) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(static_cast<char>(ch));
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

int ExtractLabelFromName(const std::string& sample_name) {
    static const std::regex label_re(R"(label_(\d+))");
    std::smatch match;
    if (!std::regex_search(sample_name, match, label_re)) {
        throw std::invalid_argument("无法从样本文件名解析真实标签: " + sample_name);
    }
    return std::stoi(match[1].str());
}

}  // namespace

SampleRepository::SampleRepository(std::string sample_dir, std::size_t feature_dim)
    : sample_dir_(std::move(sample_dir)) {
    namespace fs = std::filesystem;

    if (sample_dir_.empty()) {
        throw std::invalid_argument("样本目录不能为空");
    }

    const fs::path dir_path(sample_dir_);
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        throw std::invalid_argument("样本目录不存在: " + sample_dir_);
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const auto& file_path : files) {
        std::ifstream ifs(file_path, std::ios::binary);
        if (!ifs) {
            throw std::runtime_error("无法打开样本文件: " + file_path.string());
        }

        std::ostringstream buffer;
        buffer << ifs.rdbuf();
        BeatSample sample = ParseSampleFile(file_path.filename().string(), buffer.str(), feature_dim);
        sample_names_.push_back(sample.name);
        sample_map_.emplace(sample.name, std::move(sample));
    }

    if (sample_names_.empty()) {
        throw std::invalid_argument("样本目录中未找到可用的 .txt 文件: " + sample_dir_);
    }
}

const BeatSample* SampleRepository::FindByName(const std::string& name) const {
    const auto iter = sample_map_.find(name);
    if (iter == sample_map_.end()) {
        return nullptr;
    }
    return &iter->second;
}

BeatSample SampleRepository::ParseSampleFile(
    const std::string& sample_name,
    const std::string& content,
    std::size_t feature_dim) {
    const std::vector<std::string> tokens = Tokenize(content);
    if (tokens.size() != feature_dim && tokens.size() != feature_dim + 1) {
        std::ostringstream oss;
        oss << "样本文件格式错误: " << sample_name
            << "，期望 " << feature_dim << " 或 " << (feature_dim + 1)
            << " 个字段，实际 " << tokens.size();
        throw std::invalid_argument(oss.str());
    }

    BeatSample sample;
    sample.name = sample_name;
    sample.values.reserve(feature_dim);

    std::size_t start_index = 0;
    if (tokens.size() == feature_dim + 1) {
        try {
            sample.true_label = std::stoi(tokens.front());
        } catch (const std::exception&) {
            throw std::invalid_argument("样本真实标签不是合法整数: " + sample_name);
        }
        start_index = 1;
    } else {
        sample.true_label = ExtractLabelFromName(sample_name);
    }

    for (std::size_t i = start_index; i < tokens.size(); ++i) {
        try {
            sample.values.push_back(std::stof(tokens[i]));
        } catch (const std::exception&) {
            std::ostringstream oss;
            oss << "样本波形点不是合法数字: " << sample_name << " 第 " << i << " 列";
            throw std::invalid_argument(oss.str());
        }
    }
    return sample;
}

}  // namespace edge::service
