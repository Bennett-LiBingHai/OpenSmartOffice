#pragma once

#include "oso/base/Result.h"
#include "oso/io/IStream.h"

#include <cstdint>
#include <string>

namespace oso {

/// OOXML 写出器的抽象接口。
///
/// 定义基于元素的 XML 写出协议。调用者通过序列化的方法调用
/// 构建 XML 文档，实现类负责命名空间管理、XML 转义和输出到 IStream。
///
/// 使用示例：
/// @code
///   writer.setOutput(stream);
///   writer.writeStartDocument();
///   writer.writeStartElement(nsUri, "document");
///   writer.writeAttribute(nsUri, "attr", "value");
///   writer.writeText("Hello");
///   writer.writeEndElement();
///   writer.writeEndDocument();
/// @endcode
class IOoxmlWriter {
   public:
    virtual ~IOoxmlWriter() = default;

    /// 设置输出流。必须在调用任何 write* 方法之前调用。
    /// 流必须已打开且可写入。
    virtual void setOutput(IStream& stream) = 0;

    /// 写出 XML 声明：<?xml version="1.0" encoding="UTF-8"?>
    /// @param standalone 是否添加 standalone="yes" 属性
    virtual Result<void> writeStartDocument(bool standalone = true) = 0;

    /// 关闭文档，强制刷新所有缓冲输出并关闭所有未闭合元素。
    virtual Result<void> writeEndDocument() = 0;

    /// 开始一个元素。
    /// @param namespaceUri 元素命名空间 URI（空字符串表示无命名空间）。
    /// @param localName    元素本地名称（不含前缀）。
    ///                     限定名（prefix:localName）由实现类根据命名空间 URI 自动查找前缀构造。
    virtual Result<void> writeStartElement(const std::string& namespaceUri,
                                           const std::string& localName) = 0;

    /// 结束最近打开的、尚未关闭的元素。
    virtual Result<void> writeEndElement() = 0;

    /// 在当前打开的元素上写出一个属性。
    /// 必须在 writeStartElement 之后、文本或子元素之前调用。
    /// @param namespaceUri 属性命名空间 URI（空字符串表示无命名空间）。
    /// @param localName    属性本地名称（不含前缀）。
    /// @param value        属性值。实现类负责执行 XML 转义。
    virtual Result<void> writeAttribute(const std::string& namespaceUri,
                                        const std::string& localName, const std::string& value) = 0;

    /// 在当前元素上声明一个命名空间（xmlns:prefix="nsUri"）。
    /// 若该 URI 已在祖先元素上声明，则不重复写出。
    /// 通常在根元素上集中声明所有需要的命名空间，匹配 MS Office 行为。
    virtual Result<void> declareNamespace(const std::string& namespaceUri) = 0;

    /// 写出文本内容，自动执行 XML 转义（&lt; &gt; &amp; &quot; &apos;）。
    virtual Result<void> writeText(const std::string& text) = 0;
};

}  // namespace oso
