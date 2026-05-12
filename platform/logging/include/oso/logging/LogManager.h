#pragma once

#include <memory>
#include <sstream>
#include <string>

// spdlog 在 PRIVATE_DEPS 中，不在公开头文件中暴露。
// 初始化
// LogManager::Config cfg;
// cfg.console = true;
// cfg.logFile = "/var/log/oso/app.log";
// cfg.jsonFormat = false;
// LogManager::instance().init(cfg);

// 使用
// LOG(INFO) << "hello " << 42;
// OSO_LOG_WARN("something happened");

namespace oso {

/// 日志级别，按严重程度递增排列
enum class LogLevel {
    Debug = 0,  ///< 调试信息
    Info,       ///< 一般信息
    Warn,       ///< 警告
    Error,      ///< 错误
    Fatal,      ///< 致命错误
};

/// 日志管理器（单例），封装 spdlog 提供统一的日志输出
class LogManager {
public:
    /// 日志配置
    struct Config {
        std::string logFile;         // 文件输出路径，空则不写文件
        bool console = true;         // 控制台输出
        bool jsonFormat = false;     // JSON 结构化格式（为 Service 预留）
        bool syslogEnabled = false;  // syslog 输出（Service 模式）
        std::string syslogIdent = "oso";
        LogLevel minLevel = LogLevel::Debug;
    };

    /// 获取全局唯一实例
    static LogManager& instance();

    /// 使用指定配置初始化日志系统
    void init(const Config& config);
    /// 是否已完成初始化
    bool isInitialized() const;

    /// 记录一条日志
    /// @param level 日志级别
    /// @param file  源文件名
    /// @param line  源文件行号
    /// @param msg   日志消息
    void log(LogLevel level, const char* file, int line, const std::string& msg);
    /// 刷新所有日志输出
    void flush();

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

private:
    LogManager() = default;
    ~LogManager();

    class Impl;                       ///< 实现细节，隐藏 spdlog 头文件依赖
    std::unique_ptr<Impl> m_impl;     ///< PIMPL 指针
};

// ---- 流式日志辅助 ----
// 支持 LOG(INFO) << "hello " << 42; 语法

class LogStream {
public:
    LogStream(LogLevel level, const char* file, int line)
        : m_level(level), m_file(file), m_line(line) {}

    ~LogStream() {
        if (LogManager::instance().isInitialized()) {
            LogManager::instance().log(m_level, m_file, m_line, m_ss.str());
        }
    }

    template <typename T>
    LogStream& operator<<(const T& val) {
        m_ss << val;
        return *this;
    }

    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

private:
    LogLevel m_level;
    const char* m_file;
    int m_line;
    std::ostringstream m_ss;
};

} // namespace oso

// ---- 便捷宏 ----
// fmt 风格:  OSO_LOG_INFO("hello {}", 42);
// 流式风格:  LOG(INFO) << "hello " << 42;

#define OSO_LOG_LEVEL(level, file, line, msg) \
    do { \
        if (::oso::LogManager::instance().isInitialized()) { \
            ::oso::LogManager::instance().log(level, file, line, msg); \
        } \
    } while (0)

#define OSO_LOG_DEBUG(msg) OSO_LOG_LEVEL(::oso::LogLevel::Debug, __FILE__, __LINE__, msg)
#define OSO_LOG_INFO(msg)  OSO_LOG_LEVEL(::oso::LogLevel::Info,  __FILE__, __LINE__, msg)
#define OSO_LOG_WARN(msg)  OSO_LOG_LEVEL(::oso::LogLevel::Warn,  __FILE__, __LINE__, msg)
#define OSO_LOG_ERROR(msg) OSO_LOG_LEVEL(::oso::LogLevel::Error, __FILE__, __LINE__, msg)
#define OSO_LOG_FATAL(msg) OSO_LOG_LEVEL(::oso::LogLevel::Fatal, __FILE__, __LINE__, msg)

#define LOG(level) ::oso::LogStream(::oso::LogLevel::level, __FILE__, __LINE__)
