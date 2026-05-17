#include "oso/dom/common/DomNode.h"
#include "oso/dom/common/PropertyBag.h"
#include "oso/io/IStream.h"
#include "oso/ooxml/read/Libxml2Reader.h"
#include "oso/ooxml/write/IOoxmlWriter.h"
#include "oso/ooxml/write/Libxml2Writer.h"

#include <gtest/gtest.h>

using namespace oso;

// ================================================================
// PropertyBag
// ================================================================

TEST(PropertyBag, SetAndGet) {
    PropertyBag bag;
    bag.set("key1", "value1");
    EXPECT_EQ(bag.get("key1"), "value1");
}

TEST(PropertyBag, GetDefaultForMissingKey) {
    PropertyBag bag;
    EXPECT_EQ(bag.get("nonexistent"), "");
    EXPECT_EQ(bag.get("nonexistent", "default"), "default");
}

TEST(PropertyBag, Has) {
    PropertyBag bag;
    EXPECT_FALSE(bag.has("key"));
    bag.set("key", "val");
    EXPECT_TRUE(bag.has("key"));
}

TEST(PropertyBag, Empty) {
    PropertyBag bag;
    EXPECT_TRUE(bag.empty());
    bag.set("key", "val");
    EXPECT_FALSE(bag.empty());
}

TEST(PropertyBag, All) {
    PropertyBag bag;
    bag.set("a", "1");
    bag.set("b", "2");
    const auto& all = bag.all();
    EXPECT_EQ(all.size(), 2u);
    EXPECT_EQ(all.at("a"), "1");
    EXPECT_EQ(all.at("b"), "2");
}

TEST(PropertyBag, Overwrite) {
    PropertyBag bag;
    bag.set("key", "old");
    bag.set("key", "new");
    EXPECT_EQ(bag.get("key"), "new");
}

// ================================================================
// DomNode — 树结构
// ================================================================

TEST(DomNode, AppendChildSetsParent) {
    auto parent = std::make_unique<DomElement>("parent");
    auto child = std::make_unique<DomElement>("child");

    auto* childRaw = child.get();
    parent->appendChild(std::move(child));

    EXPECT_EQ(childRaw->parent(), parent.get());
    EXPECT_EQ(parent->childCount(), 1u);
}

TEST(DomNode, ChildAtReturnsNullForOutOfRange) {
    auto node = std::make_unique<DomElement>("root");
    EXPECT_EQ(node->childAt(0), nullptr);
    EXPECT_EQ(node->childAt(100), nullptr);
}

TEST(DomNode, ChildAtReturnsCorrectChild) {
    auto parent = std::make_unique<DomElement>("parent");
    auto child1 = std::make_unique<DomElement>("child1");
    auto child2 = std::make_unique<DomElement>("child2");

    auto* c1 = child1.get();
    auto* c2 = child2.get();

    parent->appendChild(std::move(child1));
    parent->appendChild(std::move(child2));

    EXPECT_EQ(parent->childAt(0), c1);
    EXPECT_EQ(parent->childAt(1), c2);
    EXPECT_EQ(parent->childCount(), 2u);
}

TEST(DomNode, ChildrenAreOwned) {
    auto parent = std::make_unique<DomElement>("parent");
    {
        auto child = std::make_unique<DomElement>("child");
        parent->appendChild(std::move(child));
    }
    // child 的所有权已转移给 parent，parent 销毁时自动释放
    EXPECT_EQ(parent->childCount(), 1u);
}

// ================================================================
// DomNode — 属性系统
// ================================================================

TEST(DomNode, SetAndGetAttribute) {
    DomElement node("p");
    node.setAttribute("w:rsidR", "00AB1234");
    EXPECT_EQ(node.getAttribute("w:rsidR"), "00AB1234");
    EXPECT_TRUE(node.hasAttribute("w:rsidR"));
}

TEST(DomNode, GetMissingAttributeReturnsEmpty) {
    DomElement node("p");
    EXPECT_EQ(node.getAttribute("nonexistent"), "");
    EXPECT_FALSE(node.hasAttribute("nonexistent"));
}

TEST(DomNode, MultipleAttributes) {
    DomElement node("p");
    node.setAttribute("attr1", "val1");
    node.setAttribute("attr2", "val2");
    node.setAttribute("attr3", "val3");

    const auto& attrs = node.attributes();
    EXPECT_EQ(attrs.size(), 3u);
}

TEST(DomNode, OverwriteAttribute) {
    DomElement node("p");
    node.setAttribute("key", "old");
    node.setAttribute("key", "new");
    EXPECT_EQ(node.getAttribute("key"), "new");
}

// ================================================================
// DomNode — 元数据
// ================================================================

TEST(DomNode, NodeType) {
    DomElement elem("test");
    EXPECT_EQ(elem.nodeType(), DomNodeType::Element);

    DomText text("hello");
    EXPECT_EQ(text.nodeType(), DomNodeType::Text);

    DomDocument doc("word");
    EXPECT_EQ(doc.nodeType(), DomNodeType::Document);
}

TEST(DomNode, LocalNameAndNamespace) {
    DomElement elem("p", "http://example.com/ns");
    EXPECT_EQ(elem.localName(), "p");
    EXPECT_EQ(elem.namespaceUri(), "http://example.com/ns");

    elem.setLocalName("div");
    elem.setNamespaceUri("http://example.com/ns2");
    EXPECT_EQ(elem.localName(), "div");
    EXPECT_EQ(elem.namespaceUri(), "http://example.com/ns2");
}

// ================================================================
// DomText
// ================================================================

TEST(DomText, TextContent) {
    DomText text("Hello World");
    EXPECT_EQ(text.text(), "Hello World");
}

TEST(DomText, SetText) {
    DomText text;
    text.setText("new text");
    EXPECT_EQ(text.text(), "new text");
}

TEST(DomText, AppendText) {
    DomText text("Hello");
    text.appendText(" World");
    EXPECT_EQ(text.text(), "Hello World");
}

TEST(DomText, EmptyInitialization) {
    DomText text;
    EXPECT_EQ(text.text(), "");
}

// ================================================================
// DomDocument
// ================================================================

TEST(DomDocument, DocumentType) {
    DomDocument doc("word");
    EXPECT_EQ(doc.documentType(), "word");

    doc.setDocumentType("sheet");
    EXPECT_EQ(doc.documentType(), "sheet");
}

TEST(DomDocument, DefaultDocumentType) {
    DomDocument doc;
    EXPECT_EQ(doc.documentType(), "");
}

// ================================================================
// DomNode — serialize 集成测试
// ================================================================

namespace {

void verifyRoundTrip(const DomNode& node, std::vector<std::string>& outLocalNames) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    writer.writeStartDocument();
    node.serialize(writer);
    writer.writeEndDocument();
    Libxml2Reader reader;
    reader.parse(
        stream.data(),
        [&](const std::string&, const std::string& localName, const std::string&,
            const std::vector<XmlAttribute>&) { outLocalNames.push_back(localName); },
        nullptr, nullptr);
}

}  // namespace

TEST(DomNode, SerializeSingleElement) {
    DomElement root("root");
    std::vector<std::string> names;
    verifyRoundTrip(root, names);
    EXPECT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "root");
}

TEST(DomNode, SerializeNestedElements) {
    DomElement root("root");
    root.appendChild(std::make_unique<DomElement>("child1"));
    root.appendChild(std::make_unique<DomElement>("child2"));

    std::vector<std::string> names;
    verifyRoundTrip(root, names);
    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "root");
    EXPECT_EQ(names[1], "child1");
    EXPECT_EQ(names[2], "child2");
}

TEST(DomNode, SerializeWithAttributes) {
    DomElement root("root", "http://example.com/ns");
    root.setAttribute("attr1", "val1");
    root.setAttribute("attr2", "val2");

    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);
    writer.writeStartDocument();
    root.serialize(writer);
    writer.writeEndDocument();

    Libxml2Reader reader;
    std::vector<XmlAttribute> captured;
    reader.parse(
        stream.data(),
        [&](const std::string&, const std::string& localName, const std::string&,
            const std::vector<XmlAttribute>& attrs) {
            if (localName == "root") captured = attrs;
        },
        nullptr, nullptr);

    ASSERT_EQ(captured.size(), 2u);
}

TEST(DomNode, SerializeWithText) {
    DomElement root("root");
    root.appendChild(std::make_unique<DomText>("Hello"));

    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);
    writer.writeStartDocument();
    root.serialize(writer);
    writer.writeEndDocument();

    Libxml2Reader reader;
    std::string capturedText;
    reader.parse(stream.data(), nullptr, nullptr,
                 [&](const std::string& text) { capturedText += text; });

    EXPECT_EQ(capturedText, "Hello");
}

TEST(DomNode, DeepNesting) {
    auto root = std::make_unique<DomElement>("level0");
    auto current = root.get();
    for (int i = 1; i <= 10; ++i) {
        auto child = std::make_unique<DomElement>("level" + std::to_string(i));
        auto* childRaw = child.get();
        current->appendChild(std::move(child));
        current = childRaw;
    }

    // 遍历验证链
    auto* node = root.get();
    for (int i = 0; i <= 10; ++i) {
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->localName(), "level" + std::to_string(i));
        if (i < 10) {
            ASSERT_GT(node->childCount(), 0u);
            node = dynamic_cast<DomElement*>(node->childAt(0));
        }
    }
}
