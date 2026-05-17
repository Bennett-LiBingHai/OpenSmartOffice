#pragma once

#include <cstdint>
#include <string>

namespace oso {

// 分层编码：
//   0x0000_0000 ~ 0x0FFF_FFFF  平台（Platform）
//   0x1000_0000 ~ 0x1FFF_FFFF  基础设施（Infra）
//   0x2000_0000 ~ 0x2FFF_FFFF  核心（Core）
//
// 子模块占用位 [19:16]（4 位）。
// 具体错误占用位 [15:0]。

/// 分层错误码，用于统一表示操作失败原因
class ErrorCode {
public:
    /// 错误码枚举，采用分层编码方案
    enum Value : uint32_t {
        Ok = 0,

        // ---- 平台：IO（0x0001_xxxx）----
        IOFileNotFound = 0x00010001,  // 文件未找到
        IOAccessDenied = 0x00010002,  // 访问被拒绝
        IOReadError = 0x00010003,  // 读取错误
        IOWriteError = 0x00010004,  // 写入错误
        IOUnexpectedEof = 0x00010005,  // 意外到达文件末尾

        // ---- 平台：并发（0x0002_xxxx）----
        ConcurrentQueueFull = 0x00020001,  // 队列已满
        ConcurrentShutdown = 0x00020002,  // 已关闭
        ConcurrentTimeout = 0x00020003,  // 等待超时

        // ---- 基础设施：OOXML（0x1001_xxxx）----
        OOXMLZipCorrupted = 0x10010001,  // Zip 文件损坏
        OOXMLZipEntryMissing = 0x10010002,  // Zip 条目缺失
        OOXMLXmlParseError = 0x10010003,  // XML 解析错误
        OOXMLInvalidSchema = 0x10010004,  // 无效 Schema
        OOXMLMissingPart = 0x10010005,  // 缺失 Part
        OOXMLBadContentType = 0x10010006,  // 错误的内容类型
        OOXMLZipWriteError = 0x10010007,  // Zip 写错误

        // ---- 基础设施：公式（0x1002_xxxx）----
        FormulaSyntaxError = 0x10020001,  // 语法错误
        FormulaCircularRef = 0x10020002,  // 循环引用
        FormulaUnknownFunc = 0x10020003,  // 未知函数
        FormulaTypeError = 0x10020004,  // 类型错误

        // ---- 基础设施：渲染（0x1003_xxxx）----
        RenderFontNotFound = 0x10030001,  // 字体未找到
        RenderImageDecode = 0x10030002,  // 图片解码错误

        // ---- 核心：DOM（0x2001_xxxx）----
        DOMInvalidNodeType = 0x20010001,  // 无效节点类型
        DOMInvalidOperation = 0x20010002,  // 无效操作
        DOMIndexOutOfRange = 0x20010003,  // 索引越界
        DOMDocumentCorrupted = 0x20010004,  // 文档损坏
    };

    ErrorCode() : m_value(Ok) {}  ///< 默认构造为 Ok
    // NOLINTBEGIN(google-explicit-constructor)
    ErrorCode(Value v) : m_value(v) {}  ///< 从枚举值构造
    // NOLINTEND(google-explicit-constructor)

    [[nodiscard]] Value value() const { return m_value; }  ///< 获取错误码枚举值
    [[nodiscard]] bool isOk() const { return m_value == Ok; }  ///< 是否成功
    [[nodiscard]] bool isErr() const { return m_value != Ok; }  ///< 是否失败

    [[nodiscard]] std::string toString() const;  ///< 转为可读字符串
    [[nodiscard]] uint32_t raw() const {
        return static_cast<uint32_t>(m_value);
    }  ///< 获取原始整数值

    bool operator==(const ErrorCode& o) const { return m_value == o.m_value; }  ///< 相等比较
    bool operator!=(const ErrorCode& o) const { return m_value != o.m_value; }  ///< 不等比较
    bool operator==(Value v) const { return m_value == v; }  ///< 与枚举值相等比较
    bool operator!=(Value v) const { return m_value != v; }  ///< 与枚举值不等比较

private:
    Value m_value;  ///< 错误码值
};

}  // namespace oso