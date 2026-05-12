#pragma once

#include "oso/ooxml/read/IOoxmlReader.h"
#include "oso/dom/common/DomNode.h"
#include <stack>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace oso {

// ============================================================
// OoxmlSaxHandler — SAX 事件 → DOM 树构建器
//
// 作为 IOoxmlReader 的回调接收者，将 SAX 事件流（startElement
// / endElement / characters）转换为 DOM 树。
//
// 内部维护一个元素栈：
// - startElement: 通过 ElementFactory 创建节点，压入栈，加入父节点
// - characters: 创建 DomText 节点，附加到栈顶元素
// - endElement: 出栈
//
// 使用示例：
// @code
//   OoxmlSaxHandler handler(documentType);
//   Libxml2Reader reader;
//   reader.parse(xmlData,
//       handler.onStartElement(),
//       handler.onEndElement(),
//       handler.onCharacters());
//   auto doc = handler.releaseDocument();
// @endcode
// ============================================================
class OoxmlSaxHandler {
public:
    /// 构造 SAX 处理器
    /// @param documentType 文档类型（"word"/"sheet"/"slide"），影响 ElementFactory 的路由
    explicit OoxmlSaxHandler(const std::string& documentType = "word");

    /// 获取 startElement 回调（可传给 IOoxmlReader::parse）
    IOoxmlReader::StartElementFn onStartElement();

    /// 获取 endElement 回调
    IOoxmlReader::EndElementFn onEndElement();

    /// 获取 characters 回调
    IOoxmlReader::CharactersFn onCharacters();

    /// 释放构建好的 DOM 文档，转移所有权给调用者
    /// 调用后 handler 重置为空状态，可重新使用
    std::unique_ptr<DomNode> releaseDocument();

    /// 获取构建完成的文档（不转移所有权）
    DomNode* document() const { return m_document.get(); }

private:
    std::string m_documentType;
    std::unique_ptr<DomNode> m_document;  // 文档根节点
    std::stack<DomNode*> m_stack;         // 元素栈，栈顶为当前打开的元素
};

} // namespace oso
