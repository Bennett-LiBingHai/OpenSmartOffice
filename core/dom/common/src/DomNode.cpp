#include "oso/dom/common/DomNode.h"

#include "oso/ooxml/common/OoxmlNamespaces.h"
#include "oso/ooxml/write/IOoxmlWriter.h"

namespace oso {

// ============================================================
// DomNode 实现
// ============================================================

void DomNode::appendChild(std::unique_ptr<DomNode> child) {
    child->setParent(this);
    m_children.push_back(std::move(child));
}

DomNode* DomNode::childAt(size_t index) const {
    if (index < m_children.size()) {
        return m_children[index].get();
    }
    return nullptr;
}

void DomNode::setAttribute(const std::string& qName, const std::string& value) {
    m_attributes[qName] = value;
}

const std::string& DomNode::getAttribute(const std::string& qName) const {
    static const std::string kEmpty;
    auto it = m_attributes.find(qName);
    return it != m_attributes.end() ? it->second : kEmpty;
}

bool DomNode::hasAttribute(const std::string& qName) const {
    return m_attributes.find(qName) != m_attributes.end();
}

void DomNode::serializeStartElement(IOoxmlWriter& writer) const {
    writer.writeStartElement(m_namespaceUri, m_localName);

    // 写出所有属性
    for (const auto& [qName, value] : m_attributes) {
        // 属性 qName 可能带前缀（如 "w:rsidR"），需要分解出 namespaceUri 和 localName
        auto colonPos = qName.find(':');
        if (colonPos != std::string::npos) {
            std::string prefix = qName.substr(0, colonPos);
            std::string localName = qName.substr(colonPos + 1);
            // 通过前缀查找命名空间 URI
            const auto& prefixMap = ooxml_namespaces::prefixMap();
            auto it = prefixMap.find(prefix);
            if (it != prefixMap.end()) {
                writer.writeAttribute(std::string(it->second), localName, value);
            } else {
                writer.writeAttribute("", qName, value);
            }
        } else {
            writer.writeAttribute("", qName, value);
        }
    }
}

void DomNode::serializeEndElement(IOoxmlWriter& writer) { writer.writeEndElement(); }

void DomNode::serialize(IOoxmlWriter& writer) const {
    serializeStartElement(writer);
    for (const auto& child : m_children) {
        child->serialize(writer);
    }
    serializeEndElement(writer);
}

// ============================================================
// DomText 实现
// ============================================================

void DomText::serialize(IOoxmlWriter& writer) const {
    // 文本节点不写开始/结束标签（由父元素负责），只写文本内容
    writer.writeText(m_text);
}

// ============================================================
// DomDocument 实现
// ============================================================

void DomDocument::serialize(IOoxmlWriter& writer) const {
    writer.writeStartDocument(true);
    // 只序列化子节点（跳过文档根自身，避免双重嵌套）
    for (const auto& child : m_children) {
        child->serialize(writer);
    }
    writer.writeEndDocument();
}

}  // namespace oso
