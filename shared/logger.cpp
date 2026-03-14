#include "logger.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

namespace gp::logging {
namespace {

std::tm LocalTime(std::time_t raw_time) {
    std::tm time_info{};
#ifdef _WIN32
    localtime_s(&time_info, &raw_time);
#else
    localtime_r(&raw_time, &time_info);
#endif
    return time_info;
}

}  // namespace

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

void Logger::Initialize(const LoggerOptions& options) {
    std::lock_guard<std::mutex> lock(mutex_);

    options_ = options;
    if (options_.app_name.empty()) {
        options_.app_name = "app";
    }
    if (options_.log_dir.empty()) {
        options_.log_dir = "logs";
    }

    std::error_code ec;
    std::filesystem::create_directories(options_.log_dir, ec);

    log_file_path_ = options_.log_dir / (options_.app_name + ".log");
    critical_log_file_path_ = options_.log_dir / (options_.app_name + "_critical.log");
    initialized_ = true;

    WriteUnlocked(LogLevel::Info,
                  std::string("日志系统初始化完成，主日志=") + log_file_path_.string() +
                      ", 关键日志=" + critical_log_file_path_.string());
}

bool Logger::IsInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

const std::filesystem::path& Logger::log_file_path() const {
    return log_file_path_;
}

const std::filesystem::path& Logger::critical_log_file_path() const {
    return critical_log_file_path_;
}

void Logger::Write(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    WriteUnlocked(level, message);
}

void Logger::DebugText(const std::string& message) {
    Write(LogLevel::Debug, message);
}

void Logger::InfoText(const std::string& message) {
    Write(LogLevel::Info, message);
}

void Logger::WarningText(const std::string& message) {
    Write(LogLevel::Warning, message);
}

void Logger::ErrorText(const std::string& message) {
    Write(LogLevel::Error, message);
}

void Logger::WriteUnlocked(LogLevel level, const std::string& message) {
    if (level < options_.min_level) {
        return;
    }

    const std::string line = FormatLine(level, message);

    if (options_.console_output) {
        if (level >= LogLevel::Warning) {
            std::cerr << line << std::endl;
        } else {
            std::cout << line << std::endl;
        }
    }

    if (!initialized_) {
        return;
    }

    RotateFileIfNeeded(log_file_path_, options_.max_file_size_bytes, options_.max_backup_files);
    {
        std::ofstream file(log_file_path_, std::ios::app | std::ios::binary);
        if (file.is_open()) {
            file << line << '\n';
        }
    }

    if (level >= LogLevel::Warning) {
        RotateFileIfNeeded(critical_log_file_path_,
                           options_.critical_max_file_size_bytes,
                           options_.critical_backup_files);
        std::ofstream critical_file(critical_log_file_path_, std::ios::app | std::ios::binary);
        if (critical_file.is_open()) {
            critical_file << line << '\n';
        }
    }
}

void Logger::RotateFileIfNeeded(const std::filesystem::path& file_path,
                                std::size_t max_file_size_bytes,
                                std::size_t max_backup_files) const {
    if (max_file_size_bytes == 0 || max_backup_files == 0) {
        return;
    }

    std::error_code ec;
    if (!std::filesystem::exists(file_path, ec)) {
        return;
    }

    const auto file_size = std::filesystem::file_size(file_path, ec);
    if (ec || file_size < max_file_size_bytes) {
        return;
    }

    for (std::size_t index = max_backup_files; index >= 1; --index) {
        const std::filesystem::path current_backup = file_path.string() + "." + std::to_string(index);
        if (index == max_backup_files) {
            std::filesystem::remove(current_backup, ec);
            ec.clear();
        } else {
            const std::filesystem::path next_backup = file_path.string() + "." + std::to_string(index + 1);
            if (std::filesystem::exists(current_backup, ec)) {
                std::filesystem::rename(current_backup, next_backup, ec);
                ec.clear();
            }
        }

        if (index == 1) {
            const std::filesystem::path first_backup = file_path.string() + ".1";
            std::filesystem::rename(file_path, first_backup, ec);
            ec.clear();
            break;
        }
    }
}

std::string Logger::FormatLine(LogLevel level, const std::string& message) {
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    const std::time_t raw_time = std::chrono::system_clock::to_time_t(now);
    const std::tm time_info = LocalTime(raw_time);

    std::ostringstream oss;
    oss << '[' << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S") << '.'
        << std::setw(3) << std::setfill('0') << milliseconds.count() << ']'
        << '[' << ToString(level) << ']'
        << "[tid=" << std::this_thread::get_id() << "] "
        << message;
    return oss.str();
}

const char* Logger::ToString(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARNING";
    case LogLevel::Error:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

}  // namespace gp::logging
