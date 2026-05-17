#pragma once

#include "oso/base/Result.h"
#include "oso/io/IStream.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace oso {

/// XML 元素的单个属性，包含命名空间、本地名、前缀和值。
struct XmlAttribute {
    std::string namespaceUri;  ///< 属性命名空间 URI（可能为空）。
    std::string localName;  ///< 属性的本地名称（不含前缀）。
    std::string prefix;  ///< 属性前缀（可能为空）。
    std::string value;  ///< 属性值。
};

/// OOXML 读取器的抽象接口。
///
/// 定义基于 SAX 风格回调的 XML 解析协议。实现类将原始 XML
/// 数据解析为一系列事件：开始元素、结束元素和文本内容。
/// 调用者在调用 parse() 或 parseStream() 时注册回调函数。
class IOoxmlReader {
public:
    virtual ~IOoxmlReader() = default;

    /// 开始元素回调签名。
    /// @param namespaceUri 元素命名空间 URI（无命名空间时为空字符串）。
    /// @param localName    不包含前缀的元素本地名称。
    /// @param qName        限定名称（prefix:localName，或无前缀时仅 localName）。
    /// @param attributes   该元素上的属性列表。
    using StartElementFn =
        std::function<void(const std::string& namespaceUri, const std::string& localName,
                           const std::string& qName, const std::vector<XmlAttribute>& attributes)>;

    /// 结束元素回调签名。
    /// @param namespaceUri 元素命名空间 URI（无命名空间时为空字符串）。
    /// @param localName    不包含前缀的元素本地名称。
    /// @param qName        限定名称。
    using EndElementFn = std::function<void(
        const std::string& namespaceUri, const std::string& localName, const std::string& qName)>;

    /// 文本内容回调签名。
    /// @param text 元素的文本内容。SAX 解析器可能分多次调用此回调来传递同一元素的文本片段。
    ///
    /// @warning 元素间的空白（缩进、换行）也会触发此回调。调用方应根据
    ///          元素上下文判断是否忽略纯空白文本，或检查 xml:space 属性。
    ///          接口层故意不过滤空白，以避免破坏 xml:space="preserve" 的语义。
    using CharactersFn = std::function<void(const std::string& text)>;

    /// 解析内存中的 XML 缓冲区。
    /// @param xmlData 包含原始 XML 字节的缓冲区（UTF-8 编码）。
    /// @param onStart 开始元素时调用的回调。
    /// @param onEnd   结束元素时调用的回调。
    /// @param onText  文本内容时调用的回调。
    /// @return 解析成功时返回 ok，否则返回错误。
    virtual Result<void> parse(const std::vector<uint8_t>& xmlData, StartElementFn onStart,
                               EndElementFn onEnd, CharactersFn onText) = 0;

    /// 从输入流中以流式方式解析 XML。
    ///
    /// 与 parse() 不同，此方法从 IStream 对象按需读取输入，
    /// 适用于大文件或网络来源的场景。
    /// @param stream  提供 XML 数据的输入流。
    /// @param onStart 开始元素时调用的回调。
    /// @param onEnd   结束元素时调用的回调。
    /// @param onText  文本内容时调用的回调。
    /// @return 解析成功时返回 ok，否则返回错误。
    virtual Result<void> parseStream(IStream& stream, StartElementFn onStart, EndElementFn onEnd,
                                     CharactersFn onText) = 0;
};

}  // namespace oso
