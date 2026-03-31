#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace edge::service {

// 单拍样本：一条 MIT-BIH 片段，首字段是真实标签，后续是 187 个波形点。
struct BeatSample {
    std::string name;
    int true_label = -1;
    std::vector<float> values;
};

// 样本仓库：服务启动时加载本地样本文件，给 Qt 客户端做模拟采集演示。
class SampleRepository {
public:
    SampleRepository(std::string sample_dir, std::size_t feature_dim);

    const std::vector<std::string>& ListNames() const {
        return sample_names_;
    }

    const BeatSample* FindByName(const std::string& name) const;

    const std::string& sample_dir() const {
        return sample_dir_;
    }

private:
    static BeatSample ParseSampleFile(
        const std::string& sample_name,
        const std::string& content,
        std::size_t feature_dim);

    std::string sample_dir_;
    std::vector<std::string> sample_names_;
    std::unordered_map<std::string, BeatSample> sample_map_;
};

}  // namespace edge::service
