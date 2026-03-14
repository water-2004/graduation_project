#pragma once

#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>

namespace gp::logging {

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
};

struct LoggerOptions {
    std::string app_name = "app";
    std::filesystem::path log_dir = "logs";
    LogLevel min_level = LogLevel::Debug;
    bool console_output = true;
    std::size_t max_file_size_bytes = 5 * 1024 * 1024;
    std::size_t max_backup_files = 3;
    std::size_t critical_max_file_size_bytes = 2 * 1024 * 1024;
    std::size_t critical_backup_files = 2;
};

class Logger {
public:
    static Logger& Instance();

    void Initialize(const LoggerOptions& options);
    bool IsInitialized() const;

    const std::filesystem::path& log_file_path() const;
    const std::filesystem::path& critical_log_file_path() const;

    void Write(LogLevel level, const std::string& message);
    void DebugText(const std::string& message);
    void InfoText(const std::string& message);
    void WarningText(const std::string& message);
    void ErrorText(const std::string& message);

    template <typename... Args>
    void Debug(Args&&... args) {
        Write(LogLevel::Debug, BuildMessage(std::forward<Args>(args)...));
    }

    template <typename... Args>
    void Info(Args&&... args) {
        Write(LogLevel::Info, BuildMessage(std::forward<Args>(args)...));
    }

    template <typename... Args>
    void Warning(Args&&... args) {
        Write(LogLevel::Warning, BuildMessage(std::forward<Args>(args)...));
    }

    template <typename... Args>
    void Error(Args&&... args) {
        Write(LogLevel::Error, BuildMessage(std::forward<Args>(args)...));
    }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    template <typename... Args>
    static std::string BuildMessage(Args&&... args) {
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        return oss.str();
    }

    void WriteUnlocked(LogLevel level, const std::string& message);
    void RotateFileIfNeeded(const std::filesystem::path& file_path,
                            std::size_t max_file_size_bytes,
                            std::size_t max_backup_files) const;
    static std::string FormatLine(LogLevel level, const std::string& message);
    static const char* ToString(LogLevel level);

    mutable std::mutex mutex_;
    LoggerOptions options_{};
    std::filesystem::path log_file_path_;
    std::filesystem::path critical_log_file_path_;
    bool initialized_ = false;
};

}  // namespace gp::logging
