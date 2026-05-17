#pragma once

#include "oso/ooxml/common/IZipArchive.h"

#include <memory>

struct zip;

namespace oso {

/**
 * @brief 基于 libzip 的 Zip 档案实现
 *
 * 使用 libzip 库实现 IZipArchive 接口，提供对 Zip 文件的读写访问能力。
 * 支持移动语义，禁止拷贝语义。
 */
class LibzipZipArchive : public IZipArchive {
public:
    /**
     * @brief 构造函数
     *
     * 创建一个未打开的空档案对象。
     */
    LibzipZipArchive() = default;

    /**
     * @brief 析构函数
     *
     * 自动关闭并释放底层 libzip 资源。
     */
    ~LibzipZipArchive() override;

    /// @brief 禁止拷贝构造
    LibzipZipArchive(const LibzipZipArchive&) = delete;
    /// @brief 禁止拷贝赋值
    LibzipZipArchive& operator=(const LibzipZipArchive&) = delete;

    /**
     * @brief 移动构造函数
     *
     * 转移 other 的资源所有权到新对象，other 变为空状态。
     * @param other 源对象
     */
    LibzipZipArchive(LibzipZipArchive&& other) noexcept;

    /**
     * @brief 移动赋值运算符
     *
     * 先关闭当前资源，再转移 other 的资源所有权。
     * @param other 源对象
     * @return 自身引用
     */
    LibzipZipArchive& operator=(LibzipZipArchive&& other) noexcept;

    /**
     * @brief 打开 Zip 档案（只读模式）
     *
     * @param path Zip 文件路径
     * @return 成功返回 OK；失败返回错误码（如文件不存在、非 Zip 格式）
     */
    Result<void> open(const std::string& path) override;

    /**
     * @brief 创建新 Zip 档案（写入模式）
     *
     * 若文件已存在则覆盖。创建后可使用 writeEntry() 添加条目。
     * @param path 新 Zip 文件路径
     * @return 成功返回 OK；失败返回错误码
     */
    Result<void> create(const std::string& path) override;

    /**
     * @brief 获取所有条目列表
     *
     * @return 成功返回条目数组；失败返回错误码（如档案未打开）
     */
    [[nodiscard]] Result<std::vector<Entry>> entries() const override;

    /**
     * @brief 读取指定条目内容
     *
     * @param name 条目名称（路径）
     * @return 成功返回文件内容字节数组；失败返回错误码（如条目不存在、读取失败）
     */
    Result<std::vector<uint8_t>> readEntry(const std::string& name) override;

    /**
     * @brief 向档案中写入一个条目
     *
     * 须在 create() 之后、close() 之前调用。数据自动 deflate 压缩。
     * @param name 条目名称（路径），如 "word/document.xml"
     * @param data 条目内容
     * @return 成功返回 OK；失败返回错误码（如档案未创建、写入失败）
     */
    Result<void> writeEntry(const std::string& name, const std::vector<uint8_t>& data) override;

    /**
     * @brief 关闭 Zip 档案
     *
     * 释放底层 libzip 资源，关闭后可重新调用 open。
     * @return 成功返回 OK；失败返回错误码（如关闭时 IO 错误）
     */
    Result<void> close() override;

    /**
     * @brief 检查档案是否已打开
     *
     * @return 已打开返回 true，否则返回 false
     */
    [[nodiscard]] bool isOpen() const override;

private:
    struct zip* m_archive{nullptr};  ///< libzip 底层句柄，未打开时为 nullptr
};

}  // namespace oso