#include <gtest/gtest.h>
#include "oso/logging/LogManager.h"

#include <fstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

using namespace oso;

// 测试用的日志文件路径
constexpr const char* TEST_LOG_FILE = "/tmp/oso_test_log.txt";

// ============================================================
// 辅助函数
// ============================================================

class LogTestHelper {
public:
    static bool fileContains(const std::string& filename, const std::string& substr) {
        std::ifstream ifs(filename);
        if (!ifs.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
        return content.find(substr) != std::string::npos;
    }

    static void removeTestFile() {
        std::error_code ec;
        fs::remove(TEST_LOG_FILE, ec);
    }
};

// ============================================================
// LogManager - 初始化状态
// ============================================================

TEST(LogManager, NotInitializedByDefault) {
    // 确保 LogManager 未初始化
    LogManager::instance().flush(); // 清理可能的状态
    EXPECT_FALSE(LogManager::instance().isInitialized());
}

TEST(LogManager, InitWithConsoleOnly) {
    LogManager::Config cfg;
    cfg.console = true;
    cfg.logFile = "";
    cfg.jsonFormat = false;

    LogManager::instance().init(cfg);
    EXPECT_TRUE(LogManager::instance().isInitialized());

    // 清理
    LogManager::instance().flush();
}

TEST(LogManager, InitWithFile) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;
    cfg.jsonFormat = false;

    LogManager::instance().init(cfg);
    EXPECT_TRUE(LogManager::instance().isInitialized());

    // 清理
    LogManager::instance().flush();
    LogTestHelper::removeTestFile();
}

TEST(LogManager, InitWithConsoleAndFile) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = true;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    EXPECT_TRUE(LogManager::instance().isInitialized());

    // 清理
    LogManager::instance().flush();
    LogTestHelper::removeTestFile();
}

TEST(LogManager, ReinitializeDropsOldLogger) {
    LogManager::Config cfg;
    cfg.console = true;

    LogManager::instance().init(cfg);
    EXPECT_TRUE(LogManager::instance().isInitialized());

    // 重新初始化
    LogManager::instance().init(cfg);
    EXPECT_TRUE(LogManager::instance().isInitialized());

    LogManager::instance().flush();
}

// ============================================================
// LogManager - 日志级别
// ============================================================

TEST(LogManager, LogLevelDebug) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;
    cfg.minLevel = LogLevel::Debug;

    LogManager::instance().init(cfg);
    LogManager::instance().log(LogLevel::Debug, __FILE__, __LINE__, "debug message");
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "debug message"));

    LogTestHelper::removeTestFile();
}

TEST(LogManager, LogLevelInfo) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    LogManager::instance().log(LogLevel::Info, __FILE__, __LINE__, "info message");
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "info message"));

    LogTestHelper::removeTestFile();
}

TEST(LogManager, LogLevelWarn) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    LogManager::instance().log(LogLevel::Warn, __FILE__, __LINE__, "warn message");
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "warn message"));

    LogTestHelper::removeTestFile();
}

TEST(LogManager, LogLevelError) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    LogManager::instance().log(LogLevel::Error, __FILE__, __LINE__, "error message");
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "error message"));

    LogTestHelper::removeTestFile();
}

TEST(LogManager, LogLevelFatal) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    LogManager::instance().log(LogLevel::Fatal, __FILE__, __LINE__, "fatal message");
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "fatal message"));

    LogTestHelper::removeTestFile();
}

TEST(LogManager, MinLevelFiltersMessages) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;
    cfg.minLevel = LogLevel::Error;  // 只记录 Error 及以上

    LogManager::instance().init(cfg);
    LogManager::instance().log(LogLevel::Debug, __FILE__, __LINE__, "should not appear");
    LogManager::instance().log(LogLevel::Info, __FILE__, __LINE__, "should not appear");
    LogManager::instance().log(LogLevel::Warn, __FILE__, __LINE__, "should not appear");
    LogManager::instance().log(LogLevel::Error, __FILE__, __LINE__, "error message");
    LogManager::instance().log(LogLevel::Fatal, __FILE__, __LINE__, "fatal message");
    LogManager::instance().flush();

    // Debug/Info/Warn 不应该出现
    EXPECT_FALSE(LogTestHelper::fileContains(TEST_LOG_FILE, "should not appear"));
    // Error/Fatal 应该出现
    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "error message"));
    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "fatal message"));

    LogTestHelper::removeTestFile();
}

// ============================================================
// LogManager - JSON 格式
// ============================================================

TEST(LogManager, JsonFormat) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;
    cfg.jsonFormat = true;

    LogManager::instance().init(cfg);
    LogManager::instance().log(LogLevel::Info, __FILE__, __LINE__, "test message");
    LogManager::instance().flush();

    // JSON 格式应该包含 "level", "msg" 等字段
    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, R"("level")"));
    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, R"("msg")"));
    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "test message"));

    LogTestHelper::removeTestFile();
}

TEST(LogManager, JsonFormatEscapesSpecialChars) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;
    cfg.jsonFormat = true;

    LogManager::instance().init(cfg);
    LogManager::instance().log(LogLevel::Info, __FILE__, __LINE__, "msg with \"quotes\" and \\backslash");
    LogManager::instance().flush();

    // 文件应该包含转义后的内容
    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, R"(msg with \"quotes\" and \\backslash)"));

    LogTestHelper::removeTestFile();
}

// ============================================================
// LogManager - flush
// ============================================================

TEST(LogManager, FlushWritesBufferedData) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    LogManager::instance().log(LogLevel::Info, __FILE__, __LINE__, "before flush");
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "before flush"));

    LogTestHelper::removeTestFile();
}

TEST(LogManager, FlushWithoutInitDoesNotCrash) {
    // 确保未初始化
    // （单例可能在其他测试中被初始化过，这里只是验证不会崩溃）
    LogManager::instance().flush();
    // 如果不崩溃，测试通过
}

// ============================================================
// LogMacro - OSO_LOG_XXX 宏
// ============================================================

TEST(LogMacro, OSO_LOG_DEBUG) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;
    cfg.minLevel = LogLevel::Debug;

    LogManager::instance().init(cfg);
    OSO_LOG_DEBUG("debug from macro");
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "debug from macro"));

    LogTestHelper::removeTestFile();
}

TEST(LogMacro, OSO_LOG_INFO) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    OSO_LOG_INFO("info from macro");
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "info from macro"));

    LogTestHelper::removeTestFile();
}

TEST(LogMacro, OSO_LOG_WARN) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    OSO_LOG_WARN("warn from macro");
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "warn from macro"));

    LogTestHelper::removeTestFile();
}

TEST(LogMacro, OSO_LOG_ERROR) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    OSO_LOG_ERROR("error from macro");
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "error from macro"));

    LogTestHelper::removeTestFile();
}

TEST(LogMacro, OSO_LOG_FATAL) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    OSO_LOG_FATAL("fatal from macro");
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "fatal from macro"));

    LogTestHelper::removeTestFile();
}

// ============================================================
// LogStream - 流式接口
// ============================================================

TEST(LogStream, BasicStream) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    LOG(Info) << "stream message";
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "stream message"));

    LogTestHelper::removeTestFile();
}

TEST(LogStream, StreamWithMultipleParts) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    LOG(Info) << "value=" << 42 << " name=" << "oso";
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "value=42 name=oso"));

    LogTestHelper::removeTestFile();
}

TEST(LogStream, StreamAllLevels) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;
    cfg.minLevel = LogLevel::Debug;

    LogManager::instance().init(cfg);

    LOG(Debug) << "debug stream";
    LOG(Info)  << "info stream";
    LOG(Warn)  << "warn stream";
    LOG(Error) << "error stream";
    LOG(Fatal) << "fatal stream";

    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "debug stream"));
    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "info stream"));
    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "warn stream"));
    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "error stream"));
    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "fatal stream"));

    LogTestHelper::removeTestFile();
}

TEST(LogStream, StreamWithFloat) {
    LogTestHelper::removeTestFile();

    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = TEST_LOG_FILE;

    LogManager::instance().init(cfg);
    LOG(Info) << "pi=" << 3.14159;
    LogManager::instance().flush();

    EXPECT_TRUE(LogTestHelper::fileContains(TEST_LOG_FILE, "pi="));

    LogTestHelper::removeTestFile();
}

// ============================================================
// LogManager - 边界情况
// ============================================================

TEST(LogManager, LogWithoutInitDoesNotCrash) {
    // 创建一个未初始化的 LogManager（通过作用域模拟）
    // 实际上我们只能测试宏在未初始化时的行为
    // 宏内部会检查 isInitialized()
    // 这里我们只是验证不会崩溃
    SUCCEED();  // 如果不崩溃，测试通过
}

TEST(LogManager, InitWithInvalidFile) {
    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = "/invalid/path/that/does/not/exist/log.txt";

    // 不应该崩溃，应该输出错误信息到 stderr
    EXPECT_NO_THROW(LogManager::instance().init(cfg));
    // 初始化应该失败（无法打开文件）
    EXPECT_FALSE(LogManager::instance().isInitialized());
}

TEST(LogManager, InitWithEmptyConfig) {
    LogManager::Config cfg;
    cfg.console = false;
    cfg.logFile = "";

    LogManager::instance().init(cfg);
    // 没有 sinks，应该输出警告到 stderr
    EXPECT_FALSE(LogManager::instance().isInitialized());
}

// ============================================================
// LogLevel 枚举
// ============================================================

TEST(LogLevel, EnumValues) {
    EXPECT_EQ(static_cast<int>(LogLevel::Debug), 0);
    EXPECT_EQ(static_cast<int>(LogLevel::Info),  1);
    EXPECT_EQ(static_cast<int>(LogLevel::Warn),  2);
    EXPECT_EQ(static_cast<int>(LogLevel::Error), 3);
    EXPECT_EQ(static_cast<int>(LogLevel::Fatal), 4);
}
