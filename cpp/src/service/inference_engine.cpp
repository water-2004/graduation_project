#include "service/inference_engine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t kPaperFreqBins = 17;
constexpr std::size_t kPaperTimeSteps = 11;
constexpr std::size_t kStftWindowSize = 32;
constexpr std::size_t kStftHopSize = 16;
constexpr std::size_t kFiltfiltPadLen = 21;
constexpr double kPi = 3.14159265358979323846;

constexpr std::array<double, 7> kBandpassB = {
    0.022887518404249085,
    0.0,
    -0.06866255521274725,
    0.0,
    0.06866255521274725,
    0.0,
    -0.022887518404249085,
};

constexpr std::array<double, 7> kBandpassA = {
    1.0,
    -4.627775727239177,
    8.950544684107657,
    -9.325666883829001,
    5.5548557951035376,
    -1.7959820914708,
    0.24402434949679225,
};

constexpr std::array<double, 6> kBandpassZi = {
    -0.022887518349252466,
    -0.022887518603764492,
    0.04577503710123248,
    0.045775036588352316,
    -0.02288751831889665,
    -0.0228875184176696,
};

std::vector<double> ToDoubleVector(const std::vector<float>& values) {
    return std::vector<double>(values.begin(), values.end());
}

std::vector<double> OddExtend(const std::vector<double>& values, std::size_t pad_len) {
    if (values.size() <= pad_len) {
        throw std::invalid_argument("输入长度不足，无法执行 filtfilt odd padding");
    }

    std::vector<double> extended;
    extended.reserve(values.size() + 2 * pad_len);

    const double first = values.front();
    const double last = values.back();

    for (std::size_t i = 0; i < pad_len; ++i) {
        extended.push_back(2.0 * first - values[pad_len - i]);
    }
    extended.insert(extended.end(), values.begin(), values.end());
    for (std::size_t i = 0; i < pad_len; ++i) {
        extended.push_back(2.0 * last - values[values.size() - 2 - i]);
    }
    return extended;
}

std::vector<double> ApplyIirFilter(const std::vector<double>& values, double initial_scale) {
    std::array<double, 6> state = kBandpassZi;
    for (double& item : state) {
        item *= initial_scale;
    }

    std::vector<double> output(values.size(), 0.0);
    for (std::size_t index = 0; index < values.size(); ++index) {
        const double input = values[index];
        const double y = kBandpassB[0] * input + state[0];
        for (std::size_t state_index = 0; state_index + 1 < state.size(); ++state_index) {
            state[state_index] = kBandpassB[state_index + 1] * input
                + state[state_index + 1]
                - kBandpassA[state_index + 1] * y;
        }
        state.back() = kBandpassB.back() * input - kBandpassA.back() * y;
        output[index] = y;
    }
    return output;
}

std::vector<double> ApplyFiltFiltBandpass(const std::vector<double>& values) {
    std::vector<double> extended = OddExtend(values, kFiltfiltPadLen);
    std::vector<double> forward = ApplyIirFilter(extended, extended.front());

    std::reverse(forward.begin(), forward.end());
    std::vector<double> backward = ApplyIirFilter(forward, forward.front());
    std::reverse(backward.begin(), backward.end());

    return std::vector<double>(
        backward.begin() + static_cast<std::ptrdiff_t>(kFiltfiltPadLen),
        backward.begin() + static_cast<std::ptrdiff_t>(kFiltfiltPadLen + values.size()));
}

std::vector<float> MinMaxNormalize(const std::vector<double>& values) {
    const auto [min_it, max_it] = std::minmax_element(values.begin(), values.end());
    const double min_value = *min_it;
    const double max_value = *max_it;
    const double denom = max_value - min_value;

    std::vector<float> normalized(values.size(), 0.0F);
    if (denom <= 1e-8) {
        return normalized;
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        normalized[i] = static_cast<float>((values[i] - min_value) / denom);
    }
    return normalized;
}

std::array<double, kStftWindowSize> BuildHannWindow() {
    std::array<double, kStftWindowSize> window{};
    for (std::size_t i = 0; i < kStftWindowSize; ++i) {
        window[i] = 0.5 - 0.5 * std::cos((2.0 * kPi * static_cast<double>(i))
            / static_cast<double>(kStftWindowSize - 1));
    }
    return window;
}

}  // namespace

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

    Ort::AllocatorWithDefaultOptions allocator;
    auto input_name_holder = session_.GetInputNameAllocated(0, allocator);
    auto output_name_holder = session_.GetOutputNameAllocated(0, allocator);
    input_name_ = input_name_holder.get();
    output_name_ = output_name_holder.get();

    model_input_shape_ = session_.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    if (model_input_shape_.size() == 3) {
        use_paper_reference_preprocess_ = false;
        input_mode_ = "raw_1d_cnn";
        if (model_input_shape_[1] > 0 && model_input_shape_[1] != 1) {
            throw std::invalid_argument("当前仅支持单通道 1D ONNX 输入");
        }
        if (model_input_shape_[2] > 0 && static_cast<std::size_t>(model_input_shape_[2]) != feature_dim_) {
            throw std::invalid_argument("1D ONNX 输入长度与 feature_dim 不一致");
        }
    } else if (model_input_shape_.size() == 4) {
        use_paper_reference_preprocess_ = true;
        input_mode_ = "paper_reference_stft_2dcnn";
        if (model_input_shape_[1] > 0 && model_input_shape_[1] != 1) {
            throw std::invalid_argument("当前仅支持单通道 2D ONNX 输入");
        }

        paper_freq_bins_ = model_input_shape_[2] > 0
            ? static_cast<std::size_t>(model_input_shape_[2])
            : kPaperFreqBins;
        paper_time_steps_ = model_input_shape_[3] > 0
            ? static_cast<std::size_t>(model_input_shape_[3])
            : kPaperTimeSteps;

        if (paper_freq_bins_ != kPaperFreqBins || paper_time_steps_ != kPaperTimeSteps) {
            throw std::invalid_argument("当前论文参考前处理仅支持输入形状 [N,1,17,11]");
        }
    } else {
        throw std::invalid_argument("当前仅支持 rank=3 或 rank=4 的 ONNX 输入");
    }
}

PredictResult InferenceEngine::Predict(const std::vector<float>& features) {
    if (features.size() != feature_dim_) {
        throw std::invalid_argument("输入特征维度不匹配");
    }

    auto t0 = std::chrono::steady_clock::now();
    std::vector<float> input_buffer = BuildModelInput(features);
    const std::vector<int64_t> input_shape = BuildRuntimeInputShape();

    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info_,
        input_buffer.data(),
        input_buffer.size(),
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

std::vector<float> InferenceEngine::BuildModelInput(const std::vector<float>& features) const {
    if (!use_paper_reference_preprocess_) {
        return features;
    }
    return BuildPaperReferenceSpectrogram(features);
}

std::vector<int64_t> InferenceEngine::BuildRuntimeInputShape() const {
    if (!use_paper_reference_preprocess_) {
        return {1, 1, static_cast<int64_t>(feature_dim_)};
    }
    return {
        1,
        1,
        static_cast<int64_t>(paper_freq_bins_),
        static_cast<int64_t>(paper_time_steps_),
    };
}

std::vector<float> InferenceEngine::BuildPaperReferenceSpectrogram(const std::vector<float>& features) {
    const std::vector<double> raw = ToDoubleVector(features);
    const std::vector<double> filtered = ApplyFiltFiltBandpass(raw);
    const std::vector<float> normalized = MinMaxNormalize(filtered);
    const auto hann = BuildHannWindow();

    std::vector<float> spectrogram(kPaperFreqBins * kPaperTimeSteps, 0.0F);
    float min_value = std::numeric_limits<float>::max();
    float max_value = std::numeric_limits<float>::lowest();

    for (std::size_t frame = 0; frame < kPaperTimeSteps; ++frame) {
        const std::size_t start = frame * kStftHopSize;
        std::array<double, kStftWindowSize> segment{};
        for (std::size_t n = 0; n < kStftWindowSize; ++n) {
            const std::size_t index = start + n;
            const double value = index < normalized.size() ? normalized[index] : 0.0;
            segment[n] = value * hann[n];
        }

        for (std::size_t freq = 0; freq < kPaperFreqBins; ++freq) {
            double real = 0.0;
            double imag = 0.0;
            for (std::size_t n = 0; n < kStftWindowSize; ++n) {
                const double angle = -2.0 * kPi * static_cast<double>(freq) * static_cast<double>(n)
                    / static_cast<double>(kStftWindowSize);
                real += segment[n] * std::cos(angle);
                imag += segment[n] * std::sin(angle);
            }
            const float magnitude = static_cast<float>(std::sqrt(real * real + imag * imag));
            spectrogram[freq * kPaperTimeSteps + frame] = magnitude;
            min_value = std::min(min_value, magnitude);
            max_value = std::max(max_value, magnitude);
        }
    }

    const float range = max_value - min_value;
    if (range <= 1e-8F) {
        std::fill(spectrogram.begin(), spectrogram.end(), 0.0F);
        return spectrogram;
    }

    for (float& item : spectrogram) {
        item = (item - min_value) / range;
    }
    return spectrogram;
}

std::basic_string<ORTCHAR_T> InferenceEngine::ToOrtPath(const std::string& path) {
#ifdef _WIN32
    return std::wstring(path.begin(), path.end());
#else
    return path;
#endif
}

}  // namespace edge::service

