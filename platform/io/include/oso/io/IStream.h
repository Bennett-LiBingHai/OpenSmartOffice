#pragma once

#include "oso/base/Result.h"

#include <cstdint>
#include <vector>

namespace oso {

// 指针在流中的位置
enum class StreamSeek : uint8_t {
    START = 0,  // 第一个字符位置
    CUR,  // 指针当前位置
    END  // 最后一个字符之后
};

/// 字节流抽象接口，支持读写和定位操作
class IStream {
   public:
    virtual ~IStream() = default;

    /// 读取最多 maxLen 字节到 buffer，返回实际读取的字节数
    virtual Result<size_t> read(uint8_t* buffer, size_t maxLen) = 0;

    /// 读取所有剩余字节
    virtual Result<std::vector<uint8_t>> readAll() = 0;

    /// 写入指定长度的数据
    virtual Result<void> write(const uint8_t* data, size_t len) = 0;

    /// 写入整个 vector 的数据
    Result<void> writeAll(const std::vector<uint8_t>& data) {
        return write(data.data(), data.size());
    }

    /// 移动读写指针
    /// @param offset 偏移量
    /// @param whence 基准位置
    virtual Result<uint64_t> seek(int64_t offset, StreamSeek whence) = 0;
    /// 获取当前读写位置
    virtual Result<uint64_t> tell() const = 0;

    virtual bool isOpen() const = 0;  ///< 流是否已打开
    virtual Result<void> flush() = 0;  ///< 刷新缓冲区
    virtual Result<void> close() = 0;  ///< 关闭流
    virtual Result<uint64_t> size() const = 0;  ///< 获取流的总大小
};

// 内存字节流，用于测试和小型缓冲区
class MemoryStream : public IStream {
   public:
    explicit MemoryStream(std::vector<uint8_t> data = {})
        : m_data(std::move(data)), m_pos(0), m_open(true) {
    }

    Result<size_t> read(uint8_t* buffer, size_t maxLen) override;
    Result<std::vector<uint8_t>> readAll() override;
    Result<void> write(const uint8_t* data, size_t len) override;
    Result<uint64_t> seek(int64_t offset, StreamSeek whence) override;
    Result<uint64_t> tell() const override;
    bool isOpen() const override {
        return m_open;
    }
    Result<void> flush() override {
        return Result<void>::ok();
    }
    Result<void> close() override;
    Result<uint64_t> size() const override {
        return Result<uint64_t>::ok(m_data.size());
    }

    const std::vector<uint8_t>& data() const {
        return m_data;
    }

   private:
    std::vector<uint8_t> m_data;
    size_t m_pos;
    bool m_open;
};

/// 文件字节流，封装标准 C FILE 操作。
///
/// 不可拷贝，析构时自动关闭文件。
class FileStream : public IStream {
   public:
    /// 以指定模式打开文件。
    /// @param path      文件路径
    /// @param mode      打开模式（"rb" / "wb" / "ab" 等，遵循 fopen 约定）
    explicit FileStream(const std::string& path, const char* mode = "rb");
    ~FileStream() override;

    FileStream(const FileStream&) = delete;
    FileStream& operator=(const FileStream&) = delete;
    FileStream(FileStream&&) noexcept;
    FileStream& operator=(FileStream&&) noexcept;

    Result<size_t> read(uint8_t* buffer, size_t maxLen) override;
    Result<std::vector<uint8_t>> readAll() override;
    Result<void> write(const uint8_t* data, size_t len) override;
    Result<uint64_t> seek(int64_t offset, StreamSeek whence) override;
    Result<uint64_t> tell() const override;
    bool isOpen() const override;
    Result<void> flush() override;
    Result<void> close() override;
    Result<uint64_t> size() const override;

   private:
    FILE* m_file;
};

}  // namespace oso