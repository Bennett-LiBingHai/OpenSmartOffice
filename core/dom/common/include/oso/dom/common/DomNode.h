#pragma once

#include "oso/base/Result.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oso {

// ============================================================
// DomNodeType — DOM 节点类型枚举
// ============================================================
enum class DomNodeType : uint8_t {
    Document,  // 文档根节点
    Element,  // 泛型元素节点
    Text,  // 文本节点
};

// ============================================================
// DomNode — DOM 树节点基类
//
// 所有 DOM 节点继承自此基类。每个节点持有：
// - 父节点指针和子节点列表（树形结构）
// - 完整的属性映射表，用于保留 OOXML 原始属性（保障无损读写）
// - 命名空间 URI 和本地名称
//
// 子类通过重写 nodeType() 区分类型，泛型元素统一为 DomElement。
// 未识别的属性和子元素作为 DomElement 存储，round-trip 不丢失。
// ============================================================
class DomNode {
   public:
    explicit DomNode(DomNodeType type) : m_nodeType(type) {
    }
    virtual ~DomNode() = default;

    // ---- 树结构操作 ----

    /// 获取父节点指针，根节点返回 nullptr
    DomNode* parent() const {
        return m_parent;
    }

    /// 添加子节点，自动设置子节点的父指针
    void appendChild(std::unique_ptr<DomNode> child);

    /// 获取所有子节点（只读）
    const std::vector<std::unique_ptr<DomNode>>& children() const {
        return m_children;
    }

    /// 获取子节点数量
    size_t childCount() const {
        return m_children.size();
    }

    /// 获取指定索引的子节点，越界返回 nullptr
    DomNode* childAt(size_t index) const;

    // ---- 节点元数据 ----

    /// 节点类型
    DomNodeType nodeType() const {
        return m_nodeType;
    }

    /// 节点本地名称（不含前缀），如 "p", "r", "t"
    const std::string& localName() const {
        return m_localName;
    }
    void setLocalName(const std::string& name) {
        m_localName = name;
    }

    /// 命名空间 URI（如 wordprocessingml 的完整 URI）
    const std::string& namespaceUri() const {
        return m_namespaceUri;
    }
    void setNamespaceUri(const std::string& uri) {
        m_namespaceUri = uri;
    }

    // ---- 属性系统（无损读写核心）----

    /// 设置属性（命名空间属性用 qName 如 "w:rsidR"，无命名空间属性用 localName）
    void setAttribute(const std::string& qName, const std::string& value);

    /// 获取属性值，不存在返回空字符串
    const std::string& getAttribute(const std::string& qName) const;

    /// 检查属性是否存在
    bool hasAttribute(const std::string& qName) const;

    /// 获取全部属性（用于序列化写出）
    const std::unordered_map<std::string, std::string>& attributes() const {
        return m_attributes;
    }

    // ---- 序列化 ----

    /// 将当前节点及其子树序列化为 XML，通过 writer 写出
    /// 子类重写此方法以控制元素名、属性和子节点的写出逻辑
    virtual void serialize(class IOoxmlWriter& writer) const;

   protected:
    /// 写出当前元素的开始标签（含命名空间和属性），子类在 serialize() 中调用
    void serializeStartElement(IOoxmlWriter& writer) const;

    /// 写出当前元素的结束标签
    void serializeEndElement(IOoxmlWriter& writer) const;

    /// 设置父节点（仅 appendChild 调用）
    void setParent(DomNode* parent) {
        m_parent = parent;
    }

    DomNodeType m_nodeType;
    std::string m_localName;
    std::string m_namespaceUri;
    std::unordered_map<std::string, std::string> m_attributes;  // qName->value
    DomNode* m_parent = nullptr;
    std::vector<std::unique_ptr<DomNode>> m_children;
};

// ============================================================
// DomText — 文本节点
//
// 对应 XML 中元素内的文本内容。不包含子节点。
// 如 <w:t>Hello</w:t> 中的 "Hello" 即为一个 DomText 节点。
// ============================================================
class DomText : public DomNode {
   public:
    explicit DomText(std::string text = "") : DomNode(DomNodeType::Text), m_text(std::move(text)) {
    }

    const std::string& text() const {
        return m_text;
    }
    void setText(const std::string& text) {
        m_text = text;
    }
    void appendText(const std::string& text) {
        m_text += text;
    }

    void serialize(IOoxmlWriter& writer) const override;

   private:
    std::string m_text;
};

// ============================================================
// DomElement — 泛型元素节点
//
// 用于表示未被 ElementFactory 识别为特定 DOM 类型的 OOXML 元素。
// 保留全部原始属性、命名空间和子元素，确保 round-trip 不丢失数据。
// ============================================================
class DomElement : public DomNode {
   public:
    explicit DomElement(const std::string& localName, const std::string& namespaceUri = "")
        : DomNode(DomNodeType::Element) {
        m_localName = localName;
        m_namespaceUri = namespaceUri;
    }
};

// ============================================================
// DomDocument — 文档根节点
//
// 表示一个完整的 OOXML 部件文档（如 word/document.xml）。
// 包含文档类型信息，便于保存时确定输出格式。
// ============================================================
class DomDocument : public DomNode {
   public:
    explicit DomDocument(const std::string& documentType = "")
        : DomNode(DomNodeType::Document), m_documentType(documentType) {
    }

    /// 文档类型标识（如 "word", "sheet", "slide"）
    const std::string& documentType() const {
        return m_documentType;
    }
    void setDocumentType(const std::string& type) {
        m_documentType = type;
    }

    void serialize(IOoxmlWriter& writer) const override;

   private:
    std::string m_documentType;
};

}  // namespace oso
