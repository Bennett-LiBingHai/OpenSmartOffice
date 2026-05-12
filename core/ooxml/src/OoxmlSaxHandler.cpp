#include "oso/ooxml/OoxmlSaxHandler.h"
#include"oso/ooxml/ElementFactory.h"
#include <cstring>

namespace oso {


// ============================================================
// OoxmlSaxHandler 实现
// ============================================================

OoxmlSaxHandler::OoxmlSaxHandler(const std::string& documentType)
    : m_documentType(documentType) {
    // 初始化 ElementFactory（触发静态注册）
    ElementFactory::instance();
}

IOoxmlReader::StartElementFn OoxmlSaxHandler::onStartElement() {
    return [this](const std::string& namespaceUri,
                  const std::string& localName,
                  const std::string& /*qName*/,
                  const std::vector<XmlAttribute>& attributes) {
        // 通过工厂创建 DOM 节点
        auto node = ElementFactory::instance().create(namespaceUri, localName);

        // 将 SAX 属性复制到 DOM 节点的属性表中
        for (const auto& attr : attributes) {
            std::string attrQName;
            if (!attr.prefix.empty()) {
                attrQName = attr.prefix + ":" + attr.localName;
            } else {
                attrQName = attr.localName;
            }
            node->setAttribute(attrQName, attr.value);
        }

        // 加入 DOM 树
        if (!m_document) {
            // 第一个元素即为文档根节点
            m_document = std::move(node);
            m_stack.push(m_document.get());
        } else {
            DomNode* parent = m_stack.top();
            DomNode* rawNode = node.get();
            parent->appendChild(std::move(node));
            m_stack.push(rawNode);
        }
    };
}

IOoxmlReader::EndElementFn OoxmlSaxHandler::onEndElement() {
    return [this](const std::string& /*namespaceUri*/,
                  const std::string& /*localName*/,
                  const std::string& /*qName*/) {
        if (!m_stack.empty()) {
            m_stack.pop();
        }
    };
}

IOoxmlReader::CharactersFn OoxmlSaxHandler::onCharacters() {
    return [this](const std::string& text) {
        if (m_stack.empty()) return;

        bool isWhitespaceOnly = true;
        for (unsigned char c : text) {
            if (!std::isspace(c)) {
                isWhitespaceOnly = false;
                break;
            }
        }

        DomNode* parent = m_stack.top();

        if (isWhitespaceOnly) {
            auto xmlSpace = parent->attributes().find("xml:space");
            if (xmlSpace == parent->attributes().end() || xmlSpace->second != "preserve") {
                return;
            }
        }

        // 检查是否已有相邻的文本节点，合并文本
        if (!parent->children().empty()) {
            auto* lastChild = parent->children().back().get();
            if (lastChild->nodeType() == DomNodeType::Text) {
                auto* textNode = static_cast<DomText*>(lastChild);
                textNode->appendText(text);
                return;
            }
        }

        auto textNode = std::make_unique<DomText>(text);
        parent->appendChild(std::move(textNode));
    };
}

std::unique_ptr<DomNode> OoxmlSaxHandler::releaseDocument() {
    return std::move(m_document);
}

} // namespace oso
