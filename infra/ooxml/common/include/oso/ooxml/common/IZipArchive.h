#pragma once

#include "oso/base/Result.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace oso {

// Zip 档案读写抽象接口
class IZipArchive {
   public:
    // 描述压缩包内单个文件或目录的元数据
    struct Entry {
        std::string name;  // 部件路径，如 "word/document.xml"
        uint64_t compressedSize;  // 压缩后大小
        uint64_t uncompressedSize;  // 解压后大小
        bool isDirectory;  // 是否为目录条目
    };

    virtual ~IZipArchive() = default;

    /**
     * @brief 打开 Zip 档案文件
     *
     * @param path Zip 文件路径
     * @return 成功返回 OK；失败返回对应错误码（如文件不存在、格式错误）
     */
    virtual Result<void> open(const std::string& path) = 0;

    /**
     * @brief 获取所有条目列表
     *
     * @return 成功返回条目数组；失败返回错误码（如档案未打开）
     */
    virtual Result<std::vector<Entry>> entries() const = 0;

    /**
     * @brief 读取指定条目内容
     *
     * @param name 条目名称（路径）
     * @return 成功返回文件内容字节数组；失败返回错误码（如条目不存在）
     */
    virtual Result<std::vector<uint8_t>> readEntry(const std::string& name) = 0;

    /**
     * @brief 关闭 Zip 档案
     *
     * @return 成功返回 OK；失败返回错误码（如关闭时 IO 错误）
     */
    virtual Result<void> close() = 0;

    /**
     * @brief 创建新 Zip 档案用于写入
     *
     * 若文件已存在则覆盖。创建后处于写入模式，不可调用 entries()/readEntry()。
     * @param path 新 Zip 文件路径
     * @return 成功返回 OK；失败返回错误码
     */
    virtual Result<void> create(const std::string& path) = 0;

    /**
     * @brief 向档案中写入一个条目
     *
     * 须在 create() 之后、close() 之前调用。数据自动 deflate 压缩。
     * @param name 条目名称（路径），如 "word/document.xml"
     * @param data 条目内容
     * @return 成功返回 OK；失败返回错误码（如档案未创建）
     */
    virtual Result<void> writeEntry(const std::string& name, const std::vector<uint8_t>& data) = 0;

    /**
     * @brief 检查档案是否已打开
     *
     * @return 已打开返回 true，否则返回 false
     */
    virtual bool isOpen() const = 0;
};

}  // namespace oso
