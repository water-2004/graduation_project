#include "logic/logic_system.h"
#include "net/io_context_pool.h"
#include "net/tcp_server.h"
#include "service/inference_engine.h"
#include "service/sample_repository.h"
#include "logger.h"

#include <boost/asio.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

struct Args {
    std::string model_path = "artifacts/paper_reference_cnn_full_v1/model_best.onnx";
    std::string host = "0.0.0.0";
    std::uint16_t port = 9000;
    std::size_t feature_dim = 187;
    double critical_threshold = 0.90;
    std::size_t io_threads = 4;
    std::string sample_dir;
    std::string log_dir;
};

void ConfigureUtf8Console() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

std::filesystem::path GetExecutableDir(const char* argv0) {
    std::error_code ec;
    std::filesystem::path exe_path = std::filesystem::absolute(argv0, ec);
    if (ec) {
        return std::filesystem::current_path();
    }
    if (exe_path.has_parent_path()) {
        return exe_path.parent_path();
    }
    return std::filesystem::current_path();
}

std::string ShapeToString(const std::vector<int64_t>& shape) {
    std::ostringstream oss;
    oss << '[';
    for (std::size_t i = 0; i < shape.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << shape[i];
    }
    oss << ']';
    return oss.str();
}

void PrintUsage() {
    std::cout
        << "用法: edge_infer_service [选项]\n"
        << "  --model <path>             ONNX 模型路径\n"
        << "  --host <ip>                监听地址 (默认 0.0.0.0)\n"
        << "  --port <N>                 监听端口 (默认 9000)\n"
        << "  --feature-dim <N>          原始输入特征维度 (默认 187)\n"
        << "  --critical-threshold <v>   critical 告警阈值\n"
        << "  --io-threads <N>           会话 io 线程数 (默认 4)\n"
        << "  --sample-dir <path>        模拟采集样本目录，启用 LIST_SAMPLES/PLAY_SAMPLE\n"
        << "  --log-dir <path>           日志目录 (默认 程序目录/logs)\n"
        << "\n说明:\n"
        << "  服务会根据 ONNX 输入形状自动识别模型类型。\n"
        << "  - [N,1,187] 走原始 1D-CNN 输入\n"
        << "  - [N,1,17,11] 走论文参考模型前处理: 滤波 -> 归一化 -> STFT\n"
        << "\n协议:\n"
        << "  PING\n"
        << "  PREDICT f1,f2,...,f187\n"
        << "  LIST_SAMPLES\n"
        << "  PLAY_SAMPLE <name>\n"
        << "  QUIT\n";
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
        } else if (key == "--host") {
            args.host = need_value("--host");
        } else if (key == "--port") {
            args.port = static_cast<std::uint16_t>(std::stoul(need_value("--port")));
        } else if (key == "--feature-dim") {
            args.feature_dim = static_cast<std::size_t>(std::stoul(need_value("--feature-dim")));
        } else if (key == "--critical-threshold") {
            args.critical_threshold = std::stod(need_value("--critical-threshold"));
        } else if (key == "--io-threads") {
            args.io_threads = static_cast<std::size_t>(std::stoul(need_value("--io-threads")));
            if (args.io_threads == 0) {
                throw std::invalid_argument("--io-threads 必须大于 0");
            }
        } else if (key == "--sample-dir") {
            args.sample_dir = need_value("--sample-dir");
        } else if (key == "--log-dir") {
            args.log_dir = need_value("--log-dir");
        } else if (key == "--help" || key == "-h") {
            PrintUsage();
            std::exit(0);
        } else {
            throw std::invalid_argument("未知参数: " + key);
        }
    }
    return args;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        ConfigureUtf8Console();
        const Args args = ParseArgs(argc, argv);

        gp::logging::LoggerOptions log_options;
        log_options.app_name = "edge_infer_service";
        log_options.log_dir = args.log_dir.empty()
            ? (GetExecutableDir(argv[0]) / "logs")
            : std::filesystem::path(args.log_dir);
        log_options.min_level = gp::logging::LogLevel::Debug;
        gp::logging::Logger::Instance().Initialize(log_options);

        gp::logging::Logger::Instance().Info("服务准备启动");
        gp::logging::Logger::Instance().Info("模型路径: ", args.model_path);
        gp::logging::Logger::Instance().Info("监听地址: ", args.host, ':', args.port);
        gp::logging::Logger::Instance().Info("日志目录: ", log_options.log_dir.string());

        edge::service::InferenceConfig infer_config;
        infer_config.model_path = args.model_path;
        infer_config.feature_dim = args.feature_dim;
        infer_config.critical_threshold = args.critical_threshold;

        auto engine = std::make_shared<edge::service::InferenceEngine>(infer_config);
        gp::logging::Logger::Instance().Info("模型输入模式: ", engine->input_mode());
        gp::logging::Logger::Instance().Info("模型输入形状: ", ShapeToString(engine->model_input_shape()));
        gp::logging::Logger::Instance().Info("原始输入特征维度: ", engine->feature_dim());

        std::shared_ptr<edge::service::SampleRepository> sample_repository;
        if (!args.sample_dir.empty()) {
            sample_repository = std::make_shared<edge::service::SampleRepository>(args.sample_dir, args.feature_dim);
            gp::logging::Logger::Instance().Info("样本目录: ", args.sample_dir);
            gp::logging::Logger::Instance().Info("已加载样本数: ", sample_repository->ListNames().size());
        } else {
            gp::logging::Logger::Instance().Warning("未配置样本目录，LIST_SAMPLES/PLAY_SAMPLE 命令不可用");
        }

        auto logic_system = std::make_shared<edge::logic::LogicSystem>(engine, sample_repository);

        edge::net::IOContextPool io_pool(args.io_threads);
        io_pool.Start();

        boost::asio::io_context accept_ioc;
        auto server = std::make_shared<edge::net::TcpServer>(
            accept_ioc,
            args.host,
            args.port,
            io_pool,
            logic_system);
        server->Start();

        gp::logging::Logger::Instance().Info(
            "网络层次: IOContextPool -> TcpServer -> TcpSession -> LogicSystem -> InferenceEngine");
        gp::logging::Logger::Instance().Info("服务已启动，等待客户端连接...");

        accept_ioc.run();
        io_pool.Stop();
        gp::logging::Logger::Instance().Info("服务已正常退出");
        return 0;
    } catch (const Ort::Exception& e) {
        if (gp::logging::Logger::Instance().IsInitialized()) {
            gp::logging::Logger::Instance().Error("ONNX Runtime 错误: ", e.what());
        } else {
            std::cerr << "ONNX Runtime 错误: " << e.what() << std::endl;
        }
        return 1;
    } catch (const std::exception& e) {
        if (gp::logging::Logger::Instance().IsInitialized()) {
            gp::logging::Logger::Instance().Error("程序错误: ", e.what());
        } else {
            std::cerr << "程序错误: " << e.what() << std::endl;
        }
        return 1;
    }
}
