#pragma once

#include "oso/base/Result.h"
#include "oso/io/IStream.h"

#include <memory>
#include <string>
#include <vector>

namespace oso {

/// 文件系统抽象接口，提供跨平台的文件和目录操作
class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    /// 检查路径是否存在
    [[nodiscard]] virtual bool exists(const std::string& path) const = 0;
    /// 获取文件大小（字节）
    [[nodiscard]] virtual Result<uint64_t> fileSize(const std::string& path) const = 0;
    /// 以只读方式打开文件，返回输入流
    virtual Result<std::unique_ptr<IStream>> openRead(const std::string& path) = 0;
    /// 以写入方式打开文件，返回输出流
    virtual Result<std::unique_ptr<IStream>> openWrite(const std::string& path) = 0;
    /// 删除文件
    virtual Result<void> remove(const std::string& path) = 0;
    /// 列出目录内容
    [[nodiscard]] virtual Result<std::vector<std::string>> listDirectory(
        const std::string& path) const = 0;
    /// 创建目录（包括必要的父目录）
    virtual Result<void> createDirectory(const std::string& path) = 0;
    /// 获取临时目录路径
    [[nodiscard]] virtual std::string tempPath() const = 0;
};

/// IFileSystem 的本地文件系统实现。
///
/// 基于 C++17 std::filesystem 和标准 C FILE 操作，
/// 提供跨 Linux 发行版的本地文件访问。
class NativeFileSystem : public IFileSystem {
public:
    [[nodiscard]] bool exists(const std::string& path) const override;
    [[nodiscard]] Result<uint64_t> fileSize(const std::string& path) const override;
    Result<std::unique_ptr<IStream>> openRead(const std::string& path) override;
    Result<std::unique_ptr<IStream>> openWrite(const std::string& path) override;
    Result<void> remove(const std::string& path) override;
    [[nodiscard]] Result<std::vector<std::string>> listDirectory(
        const std::string& path) const override;
    Result<void> createDirectory(const std::string& path) override;
    [[nodiscard]] std::string tempPath() const override;
};

}  // namespace oso
