#pragma once

#include "oso/dom/common/DomNode.h"

#include <memory>
#include <string>
#include <vector>

namespace oso {

// ============================================================
// WordDocument — Word 文档部件根节点
//
// 对应 word/document.xml 的 <w:document> 元素。
// 包含一个 Body 子节点，序列化时写出完整的文档 XML。
// ============================================================
class Body;
class WordDocument : public DomDocument {
public:
    WordDocument();

    /// 获取文档正文（<w:body>）
    Body* body() const;

    /// 设置文档正文，接管所有权
    void setBody(std::unique_ptr<Body> body);

    void serialize(IOoxmlWriter& writer) const override;
};

// ============================================================
// Body — 文档正文容器
//
// 对应 <w:body> 元素。包含段落、表格、节属性等块级元素。
// M1 阶段仅支持段落。
// ============================================================
class Body : public DomElement {
public:
    Body();

    /// 添加段落
    void addParagraph(std::unique_ptr<class Paragraph> para);

    /// 获取段落列表
    std::vector<Paragraph*> paragraphs() const;

    /// 获取最后一个段落（常用于添加 Run）
    Paragraph* lastParagraph() const;

    void serialize(IOoxmlWriter& writer) const override;
};

// ============================================================
// Paragraph — 段落节点
//
// 对应 <w:p> 元素。包含 Run 子节点和段落属性。
// 段落属性（<w:pPr>）通过 attributes() 保留，不单独解析。
// ============================================================
class Paragraph : public DomElement {
public:
    Paragraph();

    /// 添加 Run（文字运行）
    void addRun(std::unique_ptr<class Run> run);

    /// 获取 Run 列表
    std::vector<Run*> runs() const;

    /// 获取最后一个 Run
    Run* lastRun() const;

    void serialize(IOoxmlWriter& writer) const override;
};

// ============================================================
// Run — 文字运行节点
//
// 对应 <w:r> 元素。包含文本内容（Text）和运行属性（<w:rPr>）。
// 运行属性通过 attributes() 保留，不单独解析。
// ============================================================
class Run : public DomElement {
public:
    Run();

    /// 添加文本节点
    void addText(std::unique_ptr<class Text> text);

    /// 获取文本内容（拼接所有 Text 子节点）
    std::string textContent() const;

    /// 获取所有 Text 子节点
    std::vector<Text*> texts() const;

    void serialize(IOoxmlWriter& writer) const override;
};

// ============================================================
// Text — 文字内容节点
//
// 对应 <w:t> 元素。包含段落的实际文字内容。
// 属性 xml:space="preserve" 用于保留前导/尾随空白。
// ============================================================
class Text : public DomElement {
public:
    // 初始化时自动增加一个默认构造的DomText节点
    explicit Text();

    // 返回文本节点，没有返回nullptr
    DomText* content() const;
    void setContent(std::unique_ptr<class DomText> content);

    void serialize(IOoxmlWriter& writer) const override;
};

// ============================================================
// Section — 节属性节点
//
// 对应 <w:sectPr> 元素。包含页面尺寸、页边距、页眉/页脚引用等。
// M1 阶段作为泛型元素保留，不解构，保障 round-trip。
// ============================================================
class Section : public DomElement {
public:
    Section();
};

}  // namespace oso
