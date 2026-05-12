#include "oso/logging/LogManager.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/pattern_formatter.h>

#include <array>
#include <cstdio>

namespace oso {

// ---- internal helpers ----

namespace {

constexpr const char* kLoggerName = "oso";

constexpr std::array<const char*, 5> kLevelNames = {
    "debug", "info", "warn", "error", "fatal"
};

spdlog::level::level_enum toSpdlog(LogLevel lv) {
    switch (lv) {
        case LogLevel::Debug: return spdlog::level::debug;
        case LogLevel::Info:  return spdlog::level::info;
        case LogLevel::Warn:  return spdlog::level::warn;
        case LogLevel::Error: return spdlog::level::err;
        case LogLevel::Fatal: return spdlog::level::critical;
    }
    return spdlog::level::info;
}

// 将 file 路径截短为仅保留文件名（避免编译路径泄露）
const char* shortFile(const char* file) {
    const char* p = file;
    while (*p) ++p;
    while (p > file && *(p - 1) != '/' && *(p - 1) != '\\') --p;
    return p;
}

} // anonymous namespace

// ---- LogManager::Impl ----

class LogManager::Impl {
public:
    void init(const Config& cfg) {
        if (m_logger) {
            spdlog::drop(kLoggerName);
        }

        std::vector<spdlog::sink_ptr> sinks;

        // 控制台输出
        if (cfg.console) {
            auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            sinks.push_back(std::move(sink));
        }

        // 文件输出
        if (!cfg.logFile.empty()) {
            try {
                auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                    cfg.logFile, /*truncate=*/false);
                sinks.push_back(std::move(sink));
            } catch (const spdlog::spdlog_ex& e) {
                std::fprintf(stderr, "[oso] failed to open log file '%s': %s\n",
                             cfg.logFile.c_str(), e.what());
            }
        }

        // syslog 输出（Service 模式）
        if (cfg.syslogEnabled) {
            try {
                auto sink = std::make_shared<spdlog::sinks::syslog_sink_mt>(
                    cfg.syslogIdent, LOG_PID, LOG_USER,
                    /*enable_formatting=*/false);
                sinks.push_back(std::move(sink));
            } catch (const spdlog::spdlog_ex& e) {
                std::fprintf(stderr, "[oso] failed to open syslog: %s\n", e.what());
            }
        }

        if (sinks.empty()) {
            std::fprintf(stderr, "[oso] no log sinks configured; logging disabled\n");
            m_initialized = false;
            return;
        }

        m_logger = std::make_shared<spdlog::logger>(kLoggerName,
                                                     sinks.begin(), sinks.end());
        m_logger->set_level(toSpdlog(cfg.minLevel));
        m_logger->flush_on(spdlog::level::err);

        // 注册为默认 logger，方便 spdlog::info() 等全局函数使用
        spdlog::register_logger(m_logger);
        spdlog::set_default_logger(m_logger);

        m_jsonFormat = cfg.jsonFormat;
        m_initialized = true;
    }

    void log(LogLevel level, const char* file, int line, const std::string& msg) {
        if (!m_logger) return;

        spdlog::source_loc loc{file, line, nullptr};
        auto lv = toSpdlog(level);

        if (m_jsonFormat) {
            std::string escaped;
            escaped.reserve(msg.size() + 2);
            for (char c : msg) {
                switch (c) {
                    case '"':  escaped += R"(\")"; break;
                    case '\\': escaped += R"(\\)"; break;
                    case '\n': escaped += R"(\n)";  break;
                    case '\r': escaped += R"(\r)";  break;
                    case '\t': escaped += R"(\t)";  break;
                    default:   escaped += c;      break;
                }
            }
            m_logger->log(loc, lv,
                          R"({{"level":"{}","file":"{}","line":{},"msg":"{}"}})",
                          kLevelNames[static_cast<int>(level)],
                          shortFile(file), line, escaped);
        } else {
            m_logger->log(loc, lv, "{}", msg);
        }
    }

    void flush() {
        if (m_logger) m_logger->flush();
    }

    bool isInitialized() const { return m_initialized; }

    ~Impl() {
        if (m_logger) {
            m_logger->flush();
            spdlog::drop(kLoggerName);
        }
    }

private:
    std::shared_ptr<spdlog::logger> m_logger;
    bool m_jsonFormat = false;
    bool m_initialized = false;
};

// ---- LogManager public interface ----

LogManager& LogManager::instance() {
    static LogManager mgr;
    return mgr;
}

LogManager::~LogManager() = default;

void LogManager::init(const Config& config) {
    if (!m_impl) {
        m_impl = std::make_unique<Impl>();
    }
    m_impl->init(config);
}

bool LogManager::isInitialized() const {
    return m_impl && m_impl->isInitialized();
}

void LogManager::log(LogLevel level, const char* file, int line,
                     const std::string& msg) {
    if (m_impl) {
        m_impl->log(level, file, line, msg);
    }
}

void LogManager::flush() {
    if (m_impl) m_impl->flush();
}

} // namespace oso
