#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <onnxruntime_cxx_api.h>

namespace {

struct Args {
    std::string model_path = "artifacts/cnn_mitbih_v2/model_best.onnx";
    std::string csv_path = "MIT-BIH/mitbih_test.csv";
    std::string jsonl_path = "artifacts/edge_events_cpp.jsonl";
    std::size_t max_samples = 200;
    std::size_t feature_dim = 187;
    bool has_label = true;
    double sleep_ms = 0.0;
    double critical_threshold = 0.90;
};

struct Sample {
    std::vector<float> features;
    int true_label = -1;
};

void PrintUsage() {
    std::cout
        << "用法: edge_infer [选项]\n"
        << "  --model <path>             ONNX 模型路径\n"
        << "  --csv <path>               输入 CSV 路径\n"
        << "  --jsonl <path>             事件日志输出路径\n"
        << "  --max-samples <N>          最多推理样本数\n"
        << "  --feature-dim <N>          特征维度(默认 187)\n"
        << "  --has-label <0|1>          CSV 是否带标签列\n"
        << "  --sleep-ms <ms>            样本间隔毫秒(模拟实时)\n"
        << "  --critical-threshold <v>   critical 告警阈值\n";
}

bool ParseBool(const std::string& value) {
    if (value == "1" || value == "true" || value == "TRUE") {
        return true;
    }
    if (value == "0" || value == "false" || value == "FALSE") {
        return false;
    }
    throw std::invalid_argument("布尔参数仅支持 0/1/true/false");
}

Args ParseArgs(int argc, char* argv[]) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        auto need_value = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument(std::string("参数缺少取值: ") + name);
            }
            return argv[++i];
        };

        if (key == "--model") {
            args.model_path = need_value("--model");
        } else if (key == "--csv") {
            args.csv_path = need_value("--csv");
        } else if (key == "--jsonl") {
            args.jsonl_path = need_value("--jsonl");
        } else if (key == "--max-samples") {
            args.max_samples = static_cast<std::size_t>(std::stoul(need_value("--max-samples")));
        } else if (key == "--feature-dim") {
            args.feature_dim = static_cast<std::size_t>(std::stoul(need_value("--feature-dim")));
        } else if (key == "--has-label") {
            args.has_label = ParseBool(need_value("--has-label"));
        } else if (key == "--sleep-ms") {
            args.sleep_ms = std::stod(need_value("--sleep-ms"));
        } else if (key == "--critical-threshold") {
            args.critical_threshold = std::stod(need_value("--critical-threshold"));
        } else if (key == "--help" || key == "-h") {
            PrintUsage();
            std::exit(0);
        } else {
            throw std::invalid_argument("未知参数: " + key);
        }
    }
    return args;
}

std::vector<std::string> SplitCsvLine(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, ',')) {
        tokens.push_back(token);
    }
    return tokens;
}

bool ParseSample(const std::string& line, std::size_t feature_dim, bool has_label, Sample* sample) {
    const std::vector<std::string> tokens = SplitCsvLine(line);
    const std::size_t expected = has_label ? (feature_dim + 1) : feature_dim;
    if (tokens.size() < expected) {
        return false;
    }

    sample->features.resize(feature_dim);
    for (std::size_t i = 0; i < feature_dim; ++i) {
        sample->features[i] = std::stof(tokens[i]);
    }
    sample->true_label = -1;
    if (has_label) {
        sample->true_label = static_cast<int>(std::lround(std::stod(tokens[feature_dim])));
    }
    return true;
}

std::vector<float> Softmax(const std::vector<float>& logits) {
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

double ComputeP95(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const std::size_t idx = static_cast<std::size_t>(std::ceil(values.size() * 0.95)) - 1U;
    return values[std::min(idx, values.size() - 1U)];
}

double ComputeAccuracy(const std::vector<int>& y_true, const std::vector<int>& y_pred) {
    if (y_true.empty() || y_true.size() != y_pred.size()) {
        return 0.0;
    }
    std::size_t correct = 0;
    for (std::size_t i = 0; i < y_true.size(); ++i) {
        if (y_true[i] == y_pred[i]) {
            ++correct;
        }
    }
    return static_cast<double>(correct) / static_cast<double>(y_true.size());
}

double ComputeMacroF1(const std::vector<int>& y_true, const std::vector<int>& y_pred) {
    if (y_true.empty() || y_true.size() != y_pred.size()) {
        return 0.0;
    }

    std::set<int> classes;
    for (int v : y_true) {
        classes.insert(v);
    }
    for (int v : y_pred) {
        classes.insert(v);
    }
    if (classes.empty()) {
        return 0.0;
    }

    double macro_f1 = 0.0;
    for (int cls : classes) {
        std::size_t tp = 0;
        std::size_t fp = 0;
        std::size_t fn = 0;
        for (std::size_t i = 0; i < y_true.size(); ++i) {
            if (y_true[i] == cls && y_pred[i] == cls) {
                ++tp;
            } else if (y_true[i] != cls && y_pred[i] == cls) {
                ++fp;
            } else if (y_true[i] == cls && y_pred[i] != cls) {
                ++fn;
            }
        }

        const double precision = (tp + fp) == 0 ? 0.0 : static_cast<double>(tp) / static_cast<double>(tp + fp);
        const double recall = (tp + fn) == 0 ? 0.0 : static_cast<double>(tp) / static_cast<double>(tp + fn);
        const double f1 = (precision + recall) == 0.0 ? 0.0 : (2.0 * precision * recall) / (precision + recall);
        macro_f1 += f1;
    }
    return macro_f1 / static_cast<double>(classes.size());
}

std::string BuildAlertLevel(int pred_label, double confidence, double critical_threshold) {
    if (pred_label == 0) {
        return "normal";
    }
    if (confidence >= critical_threshold) {
        return "critical";
    }
    return "warning";
}

void ConfigureUtf8Console() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

std::basic_string<ORTCHAR_T> ToOrtPath(const std::string& path) {
#ifdef _WIN32
    return std::wstring(path.begin(), path.end());
#else
    return path;
#endif
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        ConfigureUtf8Console();
        const Args args = ParseArgs(argc, argv);

        std::ifstream csv_file(args.csv_path);
        if (!csv_file.is_open()) {
            std::cerr << "无法打开 CSV: " << args.csv_path << '\n';
            return 1;
        }

        std::ofstream jsonl_file(args.jsonl_path, std::ios::out | std::ios::trunc);
        if (!jsonl_file.is_open()) {
            std::cerr << "无法写入 JSONL: " << args.jsonl_path << '\n';
            return 1;
        }

        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "edge_infer");
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

        const auto model_path = ToOrtPath(args.model_path);
        Ort::Session session(env, model_path.c_str(), session_options);

        Ort::AllocatorWithDefaultOptions allocator;
        auto input_name_holder = session.GetInputNameAllocated(0, allocator);
        auto output_name_holder = session.GetOutputNameAllocated(0, allocator);
        const char* input_names[] = {input_name_holder.get()};
        const char* output_names[] = {output_name_holder.get()};

        const std::vector<int64_t> input_shape = {1, 1, static_cast<int64_t>(args.feature_dim)};
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<int> y_true;
        std::vector<int> y_pred;
        std::vector<double> latencies;
        std::size_t warning_count = 0;
        std::size_t critical_count = 0;

        std::string line;
        std::size_t index = 0;
        while (index < args.max_samples && std::getline(csv_file, line)) {
            if (line.empty()) {
                continue;
            }

            Sample sample;
            if (!ParseSample(line, args.feature_dim, args.has_label, &sample)) {
                std::cerr << "第 " << index << " 行解析失败，已跳过\n";
                continue;
            }

            auto t0 = std::chrono::steady_clock::now();
            Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
                memory_info,
                sample.features.data(),
                sample.features.size(),
                input_shape.data(),
                input_shape.size()
            );
            std::vector<Ort::Value> outputs = session.Run(
                Ort::RunOptions{nullptr},
                input_names,
                &input_tensor,
                1,
                output_names,
                1
            );
            auto t1 = std::chrono::steady_clock::now();
            const double latency_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            const float* logits_ptr = outputs[0].GetTensorData<float>();
            const auto out_info = outputs[0].GetTensorTypeAndShapeInfo();
            const std::size_t out_size = out_info.GetElementCount();
            std::vector<float> logits(logits_ptr, logits_ptr + out_size);
            std::vector<float> probs = Softmax(logits);
            const auto max_it = std::max_element(probs.begin(), probs.end());
            const int pred_label = static_cast<int>(std::distance(probs.begin(), max_it));
            const double confidence = static_cast<double>(*max_it);
            const std::string alert_level = BuildAlertLevel(pred_label, confidence, args.critical_threshold);

            if (alert_level == "warning") {
                ++warning_count;
            } else if (alert_level == "critical") {
                ++critical_count;
            }

            const auto now = std::chrono::system_clock::now();
            const std::time_t now_t = std::chrono::system_clock::to_time_t(now);
            std::tm now_tm{};
#ifdef _WIN32
            localtime_s(&now_tm, &now_t);
#else
            now_tm = *std::localtime(&now_t);
#endif
            std::ostringstream ts_stream;
            ts_stream << std::put_time(&now_tm, "%Y-%m-%dT%H:%M:%S");
            const std::string timestamp = ts_stream.str();

            std::cout
                << "[" << timestamp << "] idx=" << std::setw(4) << std::setfill('0') << index
                << " true=" << sample.true_label
                << " pred=" << pred_label
                << " conf=" << std::fixed << std::setprecision(3) << confidence
                << " latency=" << std::fixed << std::setprecision(3) << latency_ms << "ms"
                << " alert=" << alert_level
                << '\n';

            jsonl_file
                << "{\"timestamp\":\"" << timestamp
                << "\",\"sample_idx\":" << index
                << ",\"true_label\":" << sample.true_label
                << ",\"pred_label\":" << pred_label
                << ",\"confidence\":" << std::fixed << std::setprecision(6) << confidence
                << ",\"latency_ms\":" << std::fixed << std::setprecision(6) << latency_ms
                << ",\"alert_level\":\"" << alert_level
                << "\"}\n";

            if (sample.true_label >= 0) {
                y_true.push_back(sample.true_label);
                y_pred.push_back(pred_label);
            }
            latencies.push_back(latency_ms);
            ++index;

            if (args.sleep_ms > 0.0) {
                std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(args.sleep_ms));
            }
        }

        const double mean_latency =
            latencies.empty()
                ? 0.0
                : std::accumulate(latencies.begin(), latencies.end(), 0.0) / static_cast<double>(latencies.size());
        const double p95_latency = ComputeP95(latencies);
        const double accuracy = ComputeAccuracy(y_true, y_pred);
        const double macro_f1 = ComputeMacroF1(y_true, y_pred);

        std::cout << "\n===== 运行总结 =====\n";
        std::cout << "样本数: " << index << '\n';
        if (!y_true.empty()) {
            std::cout << "Accuracy: " << std::fixed << std::setprecision(4) << accuracy << '\n';
            std::cout << "Macro-F1: " << std::fixed << std::setprecision(4) << macro_f1 << '\n';
        } else {
            std::cout << "Accuracy: N/A (输入不含标签)\n";
            std::cout << "Macro-F1: N/A (输入不含标签)\n";
        }
        std::cout << "平均时延: " << std::fixed << std::setprecision(3) << mean_latency << " ms\n";
        std::cout << "P95时延: " << std::fixed << std::setprecision(3) << p95_latency << " ms\n";
        std::cout << "Warning数: " << warning_count << '\n';
        std::cout << "Critical数: " << critical_count << '\n';
        std::cout << "JSONL输出: " << args.jsonl_path << '\n';
        return 0;
    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime 错误: " << e.what() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "程序错误: " << e.what() << '\n';
        return 1;
    }
}
