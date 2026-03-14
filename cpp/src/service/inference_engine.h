#pragma once

#include <onnxruntime_cxx_api.h>

#include <cstddef>
#include <string>
#include <vector>

namespace edge::service {

struct InferenceConfig {
    std::string model_path;
    std::size_t feature_dim = 187;
    double critical_threshold = 0.90;
};

struct PredictResult {
    int pred_label = 0;
    double confidence = 0.0;
    std::string alert_level = "normal";
    double latency_ms = 0.0;
};

// 业务服务层：封装 ONNX Runtime 推理过程，对外只暴露 Predict 接口。
class InferenceEngine {
public:
    explicit InferenceEngine(const InferenceConfig& config);

    InferenceEngine(const InferenceEngine&) = delete;
    InferenceEngine& operator=(const InferenceEngine&) = delete;

    PredictResult Predict(const std::vector<float>& features);

    std::size_t feature_dim() const {
        return feature_dim_;
    }

private:
    static std::vector<float> Softmax(const std::vector<float>& logits);
    std::string BuildAlertLevel(int pred_label, double confidence) const;
    static std::basic_string<ORTCHAR_T> ToOrtPath(const std::string& path);

    std::size_t feature_dim_;
    double critical_threshold_;

    Ort::Env env_;
    Ort::SessionOptions session_options_;
    Ort::Session session_;
    Ort::MemoryInfo memory_info_;

    std::string input_name_;
    std::string output_name_;
};

}  // namespace edge::service
