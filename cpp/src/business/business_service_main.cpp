#include "business/business_logic.h"
#include "business/file_repository.h"
#include "net/io_context_pool.h"
#include "net/tcp_server.h"
#include "logger.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

struct Args {
    std::string host = "0.0.0.0";
    std::uint16_t port = 9200;
    std::string data_dir = "data/business_service";
    std::string log_dir;
    int thread_pool_size = 4;
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
    return exe_path.has_parent_path() ? exe_path.parent_path() : std::filesystem::current_path();
}

void PrintUsage() {
    std::cout
        << "用法: business_service [选项]\n"
        << "  --host <ip>        监听地址，默认 0.0.0.0\n"
        << "  --port <N>         监听端口，默认 9200\n"
        << "  --data-dir <path>  业务数据目录，默认 data/business_service\n"
        << "  --log-dir <path>   日志目录\n"
        << "  --threads <N>      工作线程数，默认 4\n"
        << "\n协议:\n"
        << "  PING\n"
        << "  LOGIN <username> <password>\n"
        << "  LIST_PATIENTS\n"
        << "  GET_PATIENT <patient_id>\n"
        << "  ADD_PATIENT id|name|gender|age|phone|remark\n"
        << "  UPDATE_PATIENT id|name|gender|age|phone|remark\n"
        << "  DELETE_PATIENT <patient_id>\n"
        << "  ADD_MONITOR_RECORD patient_id|pred_label|"
           "confidence|alert_level|latency_ms|"
           "source|sample_name|true_label\n"
        << "  LIST_MONITOR_RECORDS <patient_id>\n"
        << "  LIST_ALL_MONITOR_RECORDS\n"
        << "  LIST_ALERTS <patient_id>\n"
        << "  LIST_ALL_ALERTS\n"
        << "  CONFIRM_ALERT alert_id|confirmed_by\n"
        << "  CHANGE_PASSWORD username|old_password|new_password\n"
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

        if (key == "--host") {
            args.host = need_value("--host");
        } else if (key == "--port") {
            args.port = static_cast<std::uint16_t>(std::stoul(need_value("--port")));
        } else if (key == "--data-dir") {
            args.data_dir = need_value("--data-dir");
        } else if (key == "--log-dir") {
            args.log_dir = need_value("--log-dir");
        } else if (key == "--threads") {
            args.thread_pool_size = std::stoi(need_value("--threads"));
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
        log_options.app_name = "business_service";
        log_options.log_dir = args.log_dir.empty()
            ? (GetExecutableDir(argv[0]) / "logs_business")
            : std::filesystem::path(args.log_dir);
        log_options.min_level = gp::logging::LogLevel::Debug;
        gp::logging::Logger::Instance().Initialize(log_options);

        auto repository = std::make_shared<gp::backend::FileRepository>(args.data_dir);
        repository->Initialize();
        auto logic = std::make_shared<gp::backend::BusinessLogic>(repository);

        auto io_pool = std::make_shared<edge::net::IOContextPool>(
            static_cast<std::size_t>(args.thread_pool_size));

        boost::asio::io_context accept_io;
        auto server = std::make_shared<edge::net::TcpServer>(
            accept_io, args.host, args.port, *io_pool, logic);

        gp::logging::Logger::Instance().Info("业务服务启动成功");
        gp::logging::Logger::Instance().Info("监听地址: ", args.host, ':', args.port);
        gp::logging::Logger::Instance().Info("工作线程: ", args.thread_pool_size);
        gp::logging::Logger::Instance().Info("数据目录: ", args.data_dir);
        gp::logging::Logger::Instance().Info(
            "SQLite 数据库: ",
            (std::filesystem::path(args.data_dir) / "business_service.db").string());
        gp::logging::Logger::Instance().Info("默认管理员账号: admin / 123456");

        server->Start();
        io_pool->Start();
        accept_io.run();
        io_pool->Stop();
    } catch (const std::exception& e) {
        if (gp::logging::Logger::Instance().IsInitialized()) {
            gp::logging::Logger::Instance().Error("业务服务异常: ", e.what());
        } else {
            std::cerr << "业务服务异常: " << e.what() << std::endl;
        }
        return 1;
    }
}
