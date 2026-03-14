#include "service/inference_engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace edge::service {

InferenceEngine::InferenceEngine(const InferenceConfig& config)
    : feature_dim_(config.feature_dim),
      critical_threshold_(config.critical_threshold),
      env_(ORT_LOGGING_LEVEL_WARNING, "edge_infer_service"),
      session_options_(),
      session_(nullptr),
      memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
    session_options_.SetIntraOpNumThreads(1);
    session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

    const auto model_path = ToOrtPath(config.model_path);
    session_ = Ort::Session(env_, model_path.c_str(), session_options_);

    // 初始化模型输入输出节点名称，后续推理复用。
    Ort::AllocatorWithDefaultOptions allocator;
    auto input_name_holder = session_.GetInputNameAllocated(0, allocator);
    auto output_name_holder = session_.GetOutputNameAllocated(0, allocator);
    input_name_ = input_name_holder.get();
    output_name_ = output_name_holder.get();
}

PredictResult InferenceEngine::Predict(const std::vector<float>& features) {
    if (features.size() != feature_dim_) {
        throw std::invalid_argument("输入特征维度不匹配");
    }

    const std::vector<int64_t> input_shape = {1, 1, static_cast<int64_t>(feature_dim_)};
    auto* input_data = const_cast<float*>(features.data());

    auto t0 = std::chrono::steady_clock::now();
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info_,
        input_data,
        features.size(),
        input_shape.data(),
        input_shape.size());

    const char* input_names[] = {input_name_.c_str()};
    const char* output_names[] = {output_name_.c_str()};
    std::vector<Ort::Value> outputs = session_.Run(
        Ort::RunOptions{nullptr},
        input_names,
        &input_tensor,
        1,
        output_names,
        1);
    auto t1 = std::chrono::steady_clock::now();

    const float* logits_ptr = outputs[0].GetTensorData<float>();
    const std::size_t out_size = outputs[0].GetTensorTypeAndShapeInfo().GetElementCount();
    std::vector<float> logits(logits_ptr, logits_ptr + out_size);
    std::vector<float> probs = Softmax(logits);

    const auto max_it = std::max_element(probs.begin(), probs.end());

    PredictResult result;
    result.pred_label = static_cast<int>(std::distance(probs.begin(), max_it));
    result.confidence = static_cast<double>(*max_it);
    result.alert_level = BuildAlertLevel(result.pred_label, result.confidence);
    result.latency_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

std::vector<float> InferenceEngine::Softmax(const std::vector<float>& logits) {
    const float max_v = *std::max_element(logits.begin(), logits.end());
    std::vector<float> exps(logits.size(), 0.0F);
    for (std::size_t i = 0; i < logits.size(); ++i) {
        exps[i] = std::exp(logits[i] - max_v);
    }
    const float sum_exp = std::accumulate(exps.begin(), exps.end(), 0.0F);
    if (sum_exp <= 0.0F) {
        return std::vector<float>(logits.size(), 0.0F);
    }

    for (float& v : exps) {
        v /= sum_exp;
    }
    return exps;
}

std::string InferenceEngine::BuildAlertLevel(int pred_label, double confidence) const {
    if (pred_label == 0) {
        return "normal";
    }
    if (confidence >= critical_threshold_) {
        return "critical";
    }
    return "warning";
}

std::basic_string<ORTCHAR_T> InferenceEngine::ToOrtPath(const std::string& path) {
#ifdef _WIN32
    return std::wstring(path.begin(), path.end());
#else
    return path;
#endif
}

}  // namespace edge::service
