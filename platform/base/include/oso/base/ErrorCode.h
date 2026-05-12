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
        IO_FileNotFound = 0x00010001,  // 文件未找到
        IO_AccessDenied = 0x00010002,  // 访问被拒绝
        IO_ReadError = 0x00010003,  // 读取错误
        IO_WriteError = 0x00010004,  // 写入错误
        IO_UnexpectedEof = 0x00010005,  // 意外到达文件末尾

        // ---- 平台：并发（0x0002_xxxx）----
        Concurrent_QueueFull = 0x00020001,  // 队列已满
        Concurrent_Shutdown = 0x00020002,  // 已关闭
        Concurrent_Timeout = 0x00020003,  // 等待超时

        // ---- 基础设施：OOXML（0x1001_xxxx）----
        OOXML_ZipCorrupted = 0x10010001,  // Zip 文件损坏
        OOXML_ZipEntryMissing = 0x10010002,  // Zip 条目缺失
        OOXML_XmlParseError = 0x10010003,  // XML 解析错误
        OOXML_InvalidSchema = 0x10010004,  // 无效 Schema
        OOXML_MissingPart = 0x10010005,  // 缺失 Part
        OOXML_BadContentType = 0x10010006,  // 错误的内容类型
        OOXML_ZipWriteError = 0x10010007,  // Zip 写错误

        // ---- 基础设施：公式（0x1002_xxxx）----
        Formula_SyntaxError = 0x10020001,  // 语法错误
        Formula_CircularRef = 0x10020002,  // 循环引用
        Formula_UnknownFunc = 0x10020003,  // 未知函数
        Formula_TypeError = 0x10020004,  // 类型错误

        // ---- 基础设施：渲染（0x1003_xxxx）----
        Render_FontNotFound = 0x10030001,  // 字体未找到
        Render_ImageDecode = 0x10030002,  // 图片解码错误

        // ---- 核心：DOM（0x2001_xxxx）----
        DOM_InvalidNodeType = 0x20010001,  // 无效节点类型
        DOM_InvalidOperation = 0x20010002,  // 无效操作
        DOM_IndexOutOfRange = 0x20010003,  // 索引越界
        DOM_DocumentCorrupted = 0x20010004,  // 文档损坏
    };

    ErrorCode() : m_value(Ok) {
    }  ///< 默认构造为 Ok
    ErrorCode(Value v) : m_value(v) {
    }  ///< 从枚举值构造

    Value value() const {
        return m_value;
    }  ///< 获取错误码枚举值
    bool isOk() const {
        return m_value == Ok;
    }  ///< 是否成功
    bool isErr() const {
        return m_value != Ok;
    }  ///< 是否失败

    std::string toString() const;  ///< 转为可读字符串
    uint32_t raw() const {
        return static_cast<uint32_t>(m_value);
    }  ///< 获取原始整数值

    bool operator==(const ErrorCode& o) const {
        return m_value == o.m_value;
    }  ///< 相等比较
    bool operator!=(const ErrorCode& o) const {
        return m_value != o.m_value;
    }  ///< 不等比较

   private:
    Value m_value;  ///< 错误码值
};

}  // namespace oso