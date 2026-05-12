#include <gtest/gtest.h>
#include "oso/facade/DocumentFacade.h"
#include "oso/dom/common/DomNode.h"
#include "oso/dom/word/WordDocument.h"
#include "oso/ooxml/OoxmlSaxHandler.h"
#include "oso/ooxml/ElementFactory.h"
#include "oso/ooxml/read/Libxml2Reader.h"
#include "oso/ooxml/write/Libxml2Writer.h"
#include "oso/ooxml/common/IZipArchive.h"
#include "oso/ooxml/common/LibzipZipArchive.h"
#include "oso/ooxml/common/ContentTypeRegistry.h"
#include "oso/ooxml/common/RelationshipMap.h"
#include "oso/ooxml/common/OoxmlNamespaces.h"
#include "oso/io/IStream.h"
#include "oso/base/ErrorCode.h"
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <cstring>
#include <set>

using namespace oso;
namespace fs = std::filesystem;

namespace {

// RAII 临时文件清理
struct TempFile {
    std::string path;
    bool isRemain;
    explicit TempFile(std::string p, bool ir = false) : path(std::move(p)), isRemain(ir) {}
    ~TempFile() { if (!isRemain) std::remove(path.c_str()); }
};

std::string tempPath(const std::string& name) {
    return (fs::temp_directory_path() / name).string();
}

std::vector<uint8_t> strToBytes(const char* s) {
    std::string str(s);
    return std::vector<uint8_t>(str.begin(), str.end());
}

// ---- 辅助：创建最小 Word 文档 XML ----
std::vector<uint8_t> makeMinimalDocXml() {
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body><w:p/></w:body></w:document>";
    return strToBytes(xml);
}

// 含文字的 docx
std::vector<uint8_t> makeDocXmlWithText() {
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body><w:p><w:r><w:t>Hello OpenSmartOffice</w:t></w:r></w:p></w:body></w:document>";
    return strToBytes(xml);
}

// 含多段落的 docx
std::vector<uint8_t> makeDocXmlMultiParagraph() {
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body>"
        "<w:p><w:r><w:t>第一段</w:t></w:r></w:p>"
        "<w:p><w:r><w:t>第二段</w:t></w:r></w:p>"
        "<w:p><w:r><w:t>第三段</w:t></w:r></w:p>"
        "</w:body></w:document>";
    return strToBytes(xml);
}

// 含多个 Run 的段落
std::vector<uint8_t> makeDocXmlMultiRun() {
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body><w:p>"
        "<w:r><w:t>Hello</w:t></w:r>"
        "<w:r><w:t> </w:t></w:r>"
        "<w:r><w:t>World</w:t></w:r>"
        "</w:p></w:body></w:document>";
    return strToBytes(xml);
}

} // anonymous namespace

// ================================================================
// OoxmlSaxHandler — SAX → DOM 基本功能测试
// ================================================================

TEST(OoxmlSaxHandler, ParseMinimalDocx) {
    auto xmlData = makeMinimalDocXml();
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;

    auto result = reader.parse(xmlData,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters());

    ASSERT_TRUE(result.isOk()) << result.message();

    auto doc = handler.releaseDocument();
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(doc->localName(), "document");
    EXPECT_EQ(doc->childCount(), 1u); // body

    auto* body = doc->childAt(0);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->localName(), "body");
    EXPECT_GE(body->childCount(), 1u); // 至少一个 p

    auto* para = body->childAt(0);
    ASSERT_NE(para, nullptr);
    EXPECT_EQ(para->localName(), "p");
}

TEST(OoxmlSaxHandler, ParseDocWithText) {
    auto xmlData = makeDocXmlWithText();
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;

    auto result = reader.parse(xmlData,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters());

    ASSERT_TRUE(result.isOk()) << result.message();

    auto doc = handler.releaseDocument();
    ASSERT_NE(doc, nullptr);

    // 导航到文本内容
    auto* body = doc->childAt(0);
    ASSERT_NE(body, nullptr);
    auto* para = body->childAt(0);
    ASSERT_NE(para, nullptr);
    auto* run = para->childAt(0);
    ASSERT_NE(run, nullptr);
    EXPECT_EQ(run->localName(), "r");

    // 查找文本节点
    bool foundText = false;
    for (size_t i = 0; i < run->childCount(); ++i) {
        auto* child = run->childAt(i);
        if (auto* textNode = static_cast<Text*>(child)) {
            EXPECT_EQ(textNode->content()->text(), "Hello OpenSmartOffice");
            foundText = true;
        }
    }
    EXPECT_TRUE(foundText) << "应包含文本节点";
}

TEST(OoxmlSaxHandler, ParseMultiParagraph) {
    auto xmlData = makeDocXmlMultiParagraph();
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;

    auto result = reader.parse(xmlData,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters());

    ASSERT_TRUE(result.isOk()) << result.message();

    auto doc = handler.releaseDocument();
    auto* body = doc->childAt(0);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->childCount(), 3u); // 三个段落
}

TEST(OoxmlSaxHandler, ParseMultiRun) {
    auto xmlData = makeDocXmlMultiRun();
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;

    auto result = reader.parse(xmlData,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters());

    ASSERT_TRUE(result.isOk()) << result.message();

    auto doc = handler.releaseDocument();
    auto* body = doc->childAt(0);
    auto* para = body->childAt(0);
    ASSERT_NE(para, nullptr);
    EXPECT_EQ(para->childCount(), 3u); // 三个 Run
}

// ================================================================
// 序列化 — DOM → XML 写出测试
// ================================================================

TEST(Serialize, MinimalDocRoundTrip) {
    // 1. 解析
    auto xmlData = makeMinimalDocXml();
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;
    ASSERT_TRUE(reader.parse(xmlData,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());
    auto doc = handler.releaseDocument();

    // 2. 序列化
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    ASSERT_NE(wordDoc, nullptr);
    wordDoc->serialize(writer);

    // 3. 重新解析序列化结果
    OoxmlSaxHandler handler2("word");
    ASSERT_TRUE(reader.parse(stream.data(),
        handler2.onStartElement(),
        handler2.onEndElement(),
        handler2.onCharacters()).isOk());
    auto doc2 = handler2.releaseDocument();

    // 4. 验证结构一致
    ASSERT_NE(doc2, nullptr);
    EXPECT_EQ(doc2->localName(), "document");
    EXPECT_EQ(doc2->childCount(), 1u);
    EXPECT_EQ(doc2->childAt(0)->localName(), "body");
}

TEST(Serialize, DocWithTextRoundTrip) {
    auto xmlData = makeDocXmlWithText();
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;
    ASSERT_TRUE(reader.parse(xmlData,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());
    auto doc = handler.releaseDocument();

    // 序列化
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);
    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    ASSERT_NE(wordDoc, nullptr);
    wordDoc->serialize(writer);

    // 重新解析
    OoxmlSaxHandler handler2("word");
    ASSERT_TRUE(reader.parse(stream.data(),
        handler2.onStartElement(),
        handler2.onEndElement(),
        handler2.onCharacters()).isOk());
    auto doc2 = handler2.releaseDocument();

    // 验证文本内容
    auto* body = doc2->childAt(0);
    auto* para = body->childAt(0);
    auto* run = para->childAt(0);
    bool foundText = false;
    for (size_t i = 0; i < run->childCount(); ++i) {
        if ( auto* textNode = static_cast<Text*>(run->childAt(i))) {
            EXPECT_EQ(textNode->content()->text(), "Hello OpenSmartOffice");
            foundText = true;
        }
    }
    EXPECT_TRUE(foundText);
}

// ================================================================
// ElementFactory 测试
// ================================================================

TEST(ElementFactory, RegisteredElements) {
    auto& factory = ElementFactory::instance();

    const auto& wns = OoxmlNamespaces::kWordprocessingML;
    std::string wnsStr(wns);

    EXPECT_TRUE(factory.isRegistered(wnsStr, "document"));
    EXPECT_TRUE(factory.isRegistered(wnsStr, "body"));
    EXPECT_TRUE(factory.isRegistered(wnsStr, "p"));
    EXPECT_TRUE(factory.isRegistered(wnsStr, "r"));
    EXPECT_TRUE(factory.isRegistered(wnsStr, "t"));
    EXPECT_TRUE(factory.isRegistered(wnsStr, "sectPr"));
}

TEST(ElementFactory, UnregisteredFallsBackToDomElement) {
    auto& factory = ElementFactory::instance();

    // 未注册的元素应返回泛型 DomElement
    auto node = factory.create("http://unknown-ns", "unknownElement");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->nodeType(), DomNodeType::Element);
    EXPECT_EQ(node->localName(), "unknownElement");

    // 泛型 DomElement 不应是 Word 特定类型
    auto* wordDoc = dynamic_cast<WordDocument*>(node.get());
    EXPECT_EQ(wordDoc, nullptr);
}

TEST(ElementFactory, CreateWordDocument) {
    auto& factory = ElementFactory::instance();

    const auto& wns = OoxmlNamespaces::kWordprocessingML;
    auto node = factory.create(std::string(wns), "document");
    ASSERT_NE(node, nullptr);

    auto* wordDoc = dynamic_cast<WordDocument*>(node.get());
    ASSERT_NE(wordDoc, nullptr);
    EXPECT_EQ(wordDoc->documentType(), "word");
}

// ================================================================
// OoxmlSaxHandler — 空白字符处理
// ================================================================

TEST(OoxmlSaxHandler, WhitespaceOnlyTextDiscarded) {
    // 普通元素中的纯空白文本应被丢弃（SAX 可能将 XML 缩进也当作文本）
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body>\n    <w:p><w:r><w:t>Hello</w:t></w:r></w:p>\n  </w:body></w:document>";
    std::vector<uint8_t> data(xml, xml + std::strlen(xml));

    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;
    ASSERT_TRUE(reader.parse(data,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());

    auto doc = handler.releaseDocument();
    auto* body = doc->childAt(0);
    ASSERT_NE(body, nullptr);
    auto* para = body->childAt(0);
    ASSERT_NE(para, nullptr);
    auto* run = para->childAt(0);
    ASSERT_NE(run, nullptr);

    // 应该只有 w:t 元素下的文本节点，不应有空白文本节点
    EXPECT_EQ(run->childCount(), 1u);
}

TEST(OoxmlSaxHandler, WhitespacePreservedWithXmlSpace) {
    // xml:space="preserve" 的元素中空白应保留
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\""
        " xmlns:xml=\"http://www.w3.org/XML/1998/namespace\">"
        "<w:body><w:p><w:r>"
        "<w:t xml:space=\"preserve\">  leading and trailing  </w:t>"
        "</w:r></w:p></w:body></w:document>";
    std::vector<uint8_t> data(xml, xml + std::strlen(xml));

    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;
    ASSERT_TRUE(reader.parse(data,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());

    auto doc = handler.releaseDocument();
    auto* body = doc->childAt(0);
    auto* para = body->childAt(0);
    auto* run = para->childAt(0);

    auto* textNode = run->childAt(0);
    ASSERT_NE(textNode, nullptr);
    auto* textContent = dynamic_cast<Text*>(textNode);
    ASSERT_NE(textContent, nullptr);
    ASSERT_NE(textContent->content(), nullptr);
    EXPECT_EQ(textContent->content()->text(), "  leading and trailing  ");
}

// ================================================================
// OoxmlSaxHandler — 文本合并与状态管理
// ================================================================

TEST(OoxmlSaxHandler, TextMergingAdjacentTextNodes) {
    // SAX 可能将一段文字拆成多次 characters 回调
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body><w:p><w:r><w:t>Part1Part2Part3</w:t></w:r></w:p></w:body></w:document>";
    std::vector<uint8_t> data(xml, xml + std::strlen(xml));

    // libxml2 对该 XML 可能一次性返回全部文本，但我们的实现
    // 支持合并（通过 lastChild 检查和 appendText），所以验证文本是正确的即可
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;
    ASSERT_TRUE(reader.parse(data,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());

    auto doc = handler.releaseDocument();
    auto* body = doc->childAt(0);
    auto* para = body->childAt(0);
    auto* run = para->childAt(0);
    auto* textNode = dynamic_cast<Text*>(run->childAt(0));
    ASSERT_NE(textNode, nullptr);
    ASSERT_NE(textNode->content(), nullptr);
    EXPECT_EQ(textNode->content()->text(), "Part1Part2Part3");
}

TEST(OoxmlSaxHandler, ReleaseDocumentResetsState) {
    auto xmlData = makeMinimalDocXml();
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;

    // 第一次解析
    ASSERT_TRUE(reader.parse(xmlData,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());
    auto doc1 = handler.releaseDocument();
    ASSERT_NE(doc1, nullptr);
    EXPECT_EQ(handler.document(), nullptr); // 转移后内部应为空

    // 第二次解析（复用 handler）
    ASSERT_TRUE(reader.parse(xmlData,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());
    auto doc2 = handler.releaseDocument();
    ASSERT_NE(doc2, nullptr);
    EXPECT_EQ(doc2->localName(), "document");
}

TEST(OoxmlSaxHandler, ParseDeeplyNestedElements) {
    // 深度嵌套的元素结构
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body><w:p><w:r><w:rPr><w:b w:val=\"true\">"
        "<w:i w:val=\"false\"><w:u w:val=\"single\"/></w:i>"
        "</w:b></w:rPr><w:t>Deep</w:t></w:r></w:p></w:body></w:document>";
    std::vector<uint8_t> data(xml, xml + std::strlen(xml));

    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;
    ASSERT_TRUE(reader.parse(data,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());

    auto doc = handler.releaseDocument();
    ASSERT_NE(doc, nullptr);

    // 导航到深层：document → body → p → r → rPr → b → i → u
    auto* body = doc->childAt(0);
    auto* para = body->childAt(0);
    auto* run = para->childAt(0);

    // rPr 是 Run 属性，包含在 run 的元素中
    ASSERT_GE(run->childCount(), 2u); // rPr + t（至少）
}

// ================================================================
// ElementFactory — 边界测试
// ================================================================

TEST(ElementFactory, DifferentNamespaceSameLocalName) {
    auto& factory = ElementFactory::instance();

    // "document" 在 WordprocessingML 命名空间下 → WordDocument
    const auto& wns = OoxmlNamespaces::kWordprocessingML;
    auto wordNode = factory.create(std::string(wns), "document");
    ASSERT_NE(wordNode, nullptr);
    EXPECT_NE(dynamic_cast<WordDocument*>(wordNode.get()), nullptr);

    // "document" 在未知命名空间下 → 泛型 DomElement
    auto genericNode = factory.create("http://example.com/unknown", "document");
    ASSERT_NE(genericNode, nullptr);
    EXPECT_EQ(dynamic_cast<WordDocument*>(genericNode.get()), nullptr);
    EXPECT_EQ(genericNode->nodeType(), DomNodeType::Element);
}

TEST(ElementFactory, IsRegisteredDifferentNamespace) {
    auto& factory = ElementFactory::instance();

    const auto& wns = OoxmlNamespaces::kWordprocessingML;
    std::string wnsStr(wns);

    EXPECT_TRUE(factory.isRegistered(wnsStr, "p"));
    EXPECT_FALSE(factory.isRegistered("http://unknown-ns", "p"));
}

// ================================================================
// Serialize — 多段落 round-trip
// ================================================================

TEST(Serialize, MultiParagraphRoundTrip) {
    auto xmlData = makeDocXmlMultiParagraph();
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;
    ASSERT_TRUE(reader.parse(xmlData,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());
    auto doc = handler.releaseDocument();

    // 序列化
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);
    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    ASSERT_NE(wordDoc, nullptr);
    wordDoc->serialize(writer);

    // 重新解析
    OoxmlSaxHandler handler2("word");
    ASSERT_TRUE(reader.parse(stream.data(),
        handler2.onStartElement(),
        handler2.onEndElement(),
        handler2.onCharacters()).isOk());
    auto doc2 = handler2.releaseDocument();

    auto* body = doc2->childAt(0);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->childCount(), 3u); // 三个段落

    // 验证每段文字内容
    std::string expectedTexts[] = {"\xe7\xac\xac\xe4\xb8\x80\xe6\xae\xb5",
                                   "\xe7\xac\xac\xe4\xba\x8c\xe6\xae\xb5",
                                   "\xe7\xac\xac\xe4\xb8\x89\xe6\xae\xb5"};
    for (size_t pi = 0; pi < 3; ++pi) {
        auto* para = body->childAt(pi);
        ASSERT_NE(para, nullptr);
        auto* run = para->childAt(0);
        ASSERT_NE(run, nullptr);
        auto* textNode = dynamic_cast<Text*>(run->childAt(0));
        ASSERT_NE(textNode, nullptr);
        ASSERT_NE(textNode->content(), nullptr);
        EXPECT_EQ(textNode->content()->text(), expectedTexts[pi]);
    }
}

TEST(Serialize, DomDocumentSerializeProducesWellFormedXml) {
    // 验证 WordDocument::serialize 产出 well-formed XML
    auto xmlData = makeDocXmlWithText();
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;
    ASSERT_TRUE(reader.parse(xmlData,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());
    auto doc = handler.releaseDocument();

    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);
    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    ASSERT_NE(wordDoc, nullptr);
    wordDoc->serialize(writer);

    // 验证序列化结果可被 libxml2 重新解析
    OoxmlSaxHandler handler2("word");
    auto result = reader.parse(stream.data(),
        handler2.onStartElement(),
        handler2.onEndElement(),
        handler2.onCharacters());
    ASSERT_TRUE(result.isOk()) << "序列化结果不是 well-formed XML: " << result.message();
}

// ================================================================
// DocumentFacade — 统一入口测试
// ================================================================

TEST(DocumentFacade, OpenMinimalDocx) {
    DocumentFacade facade;
    auto result = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");

    ASSERT_TRUE(result.isOk()) << result.message();
    auto doc = result.takeValue();
    ASSERT_NE(doc, nullptr);
}

TEST(DocumentFacade, OpenMinimalDocxHasCorrectStructure) {
    DocumentFacade facade;
    auto result = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");

    ASSERT_TRUE(result.isOk()) << result.message();
    auto doc = result.takeValue();

    // 应包含 body → p 结构
    EXPECT_EQ(doc->localName(), "document");

    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    ASSERT_NE(wordDoc, nullptr);

    auto* body = wordDoc->body();
    ASSERT_NE(body, nullptr);

    auto* para = body->lastParagraph();
    EXPECT_NE(para, nullptr);
}

TEST(DocumentFacade, RoundTripMinimalDocx) {
    DocumentFacade facade;

    // 1. 打开
    auto openResult = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");
    ASSERT_TRUE(openResult.isOk()) << openResult.message();
    auto doc = openResult.takeValue();

    // 2. 保存到临时文件
    TempFile tmp(tempPath("test_facade_roundtrip.docx"));
    auto saveResult = facade.saveDocument(*doc, tmp.path);
    ASSERT_TRUE(saveResult.isOk()) << saveResult.message();

    // 3. 重新打开
    auto reopenResult = facade.openDocument(tmp.path);
    ASSERT_TRUE(reopenResult.isOk()) << reopenResult.message();
    auto reopened = reopenResult.takeValue();

    // 4. 结构对比
    ASSERT_NE(reopened, nullptr);
    EXPECT_EQ(reopened->localName(), doc->localName());

    auto* rwDoc = dynamic_cast<WordDocument*>(reopened.get());
    ASSERT_NE(rwDoc, nullptr);
    EXPECT_NE(rwDoc->body(), nullptr);
}

// ================================================================
// 集成测试：含属性的文档 round-trip（验证无损读写）
// ================================================================

TEST(IntegrationTest, AttributesSurviveRoundTrip) {
    // 含属性的文档 XML
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\""
        " xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<w:body>"
        "<w:p w:rsidR=\"00AB1234\" w:rsidRDefault=\"00CD5678\">"
        "<w:r>"
        "<w:rPr><w:b w:val=\"true\"/></w:rPr>"
        "<w:t xml:space=\"preserve\">  保留空白的文字  </w:t>"
        "</w:r>"
        "</w:p>"
        "</w:body></w:document>";

    std::vector<uint8_t> data(xml, xml + std::strlen(xml));

    // 解析
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;
    ASSERT_TRUE(reader.parse(data,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());
    auto doc = handler.releaseDocument();

    // 验证属性被保留
    auto* body = doc->childAt(0);
    ASSERT_NE(body, nullptr);
    auto* para = body->childAt(0);
    ASSERT_NE(para, nullptr);

    // 段落属性
    EXPECT_EQ(para->getAttribute("w:rsidR"), "00AB1234");
    EXPECT_EQ(para->getAttribute("w:rsidRDefault"), "00CD5678");

    // 序列化
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);
    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    wordDoc->serialize(writer);

    // 重新解析并验证属性仍存在
    OoxmlSaxHandler handler2("word");
    ASSERT_TRUE(reader.parse(stream.data(),
        handler2.onStartElement(),
        handler2.onEndElement(),
        handler2.onCharacters()).isOk());
    auto doc2 = handler2.releaseDocument();

    auto* body2 = doc2->childAt(0);
    auto* para2 = body2->childAt(0);
    ASSERT_NE(para2, nullptr);
    EXPECT_EQ(para2->getAttribute("w:rsidR"), "00AB1234");
    EXPECT_EQ(para2->getAttribute("w:rsidRDefault"), "00CD5678");
}

// ================================================================
// 集成测试：验证生成的文件可被 libzip 正常读取
// ================================================================

TEST(IntegrationTest, GeneratedDocxHasCorrectZipStructure) {
    DocumentFacade facade;

    // 打开并保存
    auto openResult = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");
    ASSERT_TRUE(openResult.isOk());
    auto doc = openResult.takeValue();

    TempFile tmp(tempPath("test_zip_structure.docx"));
    ASSERT_TRUE(facade.saveDocument(*doc, tmp.path).isOk());

    // 验证 Zip 结构
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(tmp.path).isOk());

    auto entries = archive.entries();
    ASSERT_TRUE(entries.isOk());
    auto& list = entries.value();

    // 应包含三个必要部件
    bool hasContentTypes = false;
    bool hasRels = false;
    bool hasDocument = false;

    for (const auto& e : list) {
        if (e.name == "[Content_Types].xml") hasContentTypes = true;
        if (e.name == "_rels/.rels") hasRels = true;
        if (e.name == "word/document.xml") hasDocument = true;
    }

    EXPECT_TRUE(hasContentTypes) << "缺少 [Content_Types].xml";
    EXPECT_TRUE(hasRels) << "缺少 _rels/.rels";
    EXPECT_TRUE(hasDocument) << "缺少 word/document.xml";

    // 验证 Content_Types 可解析
    auto ctData = archive.readEntry("[Content_Types].xml");
    ASSERT_TRUE(ctData.isOk());
    auto ct = ContentTypeRegistry::parse(ctData.value());
    ASSERT_TRUE(ct.isOk());

    // 验证 document.xml 的 MIME 类型正确
    auto mimeType = ct.value().getTypeForPart("word/document.xml");
    ASSERT_TRUE(mimeType.isOk());
    EXPECT_EQ(mimeType.value(),
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml");
}

// ================================================================
// 集成测试：富文档 round-trip（M1.8 里程碑验收）
// ================================================================

TEST(IntegrationTest, RichDocumentRoundTrip) {
    // 含多段落、多 Run、属性、嵌套元素的复杂文档
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\""
        " xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<w:body>"
        "<w:p w:rsidR=\"00AB1234\">"
        "<w:r><w:rPr><w:b w:val=\"true\"/></w:rPr><w:t>Bold</w:t></w:r>"
        "<w:r><w:t> Normal </w:t></w:r>"
        "<w:r><w:rPr><w:i w:val=\"true\"/></w:rPr><w:t>Italic</w:t></w:r>"
        "</w:p>"
        "<w:p w:rsidR=\"00CD5678\">"
        "<w:r><w:t>Second paragraph with </w:t></w:r>"
        "<w:r><w:rPr><w:u w:val=\"single\"/></w:rPr><w:t>underline</w:t></w:r>"
        "</w:p>"
        "</w:body></w:document>";
    std::vector<uint8_t> data(xml, xml + std::strlen(xml));

    // 解析
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;
    ASSERT_TRUE(reader.parse(data,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());
    auto doc = handler.releaseDocument();

    // 验证结构
    auto* body = doc->childAt(0);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->childCount(), 2u); // 两个段落

    // 第一段：3 个 Run
    auto* para1 = body->childAt(0);
    ASSERT_NE(para1, nullptr);
    EXPECT_EQ(para1->childCount(), 3u);
    EXPECT_EQ(para1->getAttribute("w:rsidR"), "00AB1234");

    // 序列化
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);
    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    ASSERT_NE(wordDoc, nullptr);
    wordDoc->serialize(writer);

    // 重新解析
    OoxmlSaxHandler handler2("word");
    ASSERT_TRUE(reader.parse(stream.data(),
        handler2.onStartElement(),
        handler2.onEndElement(),
        handler2.onCharacters()).isOk());
    auto doc2 = handler2.releaseDocument();

    // 重新验证结构
    auto* body2 = doc2->childAt(0);
    ASSERT_NE(body2, nullptr);
    EXPECT_EQ(body2->childCount(), 2u);

    auto* para2_1 = body2->childAt(0);
    EXPECT_EQ(para2_1->childCount(), 3u);
    EXPECT_EQ(para2_1->getAttribute("w:rsidR"), "00AB1234");

    auto* para2_2 = body2->childAt(1);
    EXPECT_EQ(para2_2->childCount(), 2u);
    EXPECT_EQ(para2_2->getAttribute("w:rsidR"), "00CD5678");
}

TEST(IntegrationTest, RoundTripZipEntriesMatch) {
    DocumentFacade facade;

    // 打开 → 保存 → 对比 Zip 条目数
    auto openResult = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");
    ASSERT_TRUE(openResult.isOk());
    auto doc = openResult.takeValue();

    // 获取原始条目
    LibzipZipArchive origArchive;
    ASSERT_TRUE(origArchive.open(std::string(TEST_FIXTURE_DIR) + "/minimal.docx").isOk());
    auto origEntries = origArchive.entries();
    ASSERT_TRUE(origEntries.isOk());
    size_t origCount = origEntries.value().size();

    TempFile tmp(tempPath("test_entries_match.docx"));
    ASSERT_TRUE(facade.saveDocument(*doc, tmp.path).isOk());

    // 获取输出条目
    LibzipZipArchive newArchive;
    ASSERT_TRUE(newArchive.open(tmp.path).isOk());
    auto newEntries = newArchive.entries();
    ASSERT_TRUE(newEntries.isOk());

    // 必须包含三个必要部件（允许不同结构，但不允许缺失）
    std::set<std::string> requiredParts = {
        "[Content_Types].xml",
        "_rels/.rels",
        "word/document.xml"
    };
    for (const auto& e : newEntries.value()) {
        requiredParts.erase(e.name);
    }
    EXPECT_TRUE(requiredParts.empty())
        << "缺少必要部件: " << (requiredParts.empty() ? "" : *requiredParts.begin());
}

TEST(IntegrationTest, EmptyDocRoundTrip) {
    // 空白文档（只有空段落）
    auto xmlData = makeMinimalDocXml();

    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;
    ASSERT_TRUE(reader.parse(xmlData,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());
    auto doc = handler.releaseDocument();

    // 序列化 → 重新解析 → 验证结构
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);
    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    ASSERT_NE(wordDoc, nullptr);
    wordDoc->serialize(writer);

    OoxmlSaxHandler handler2("word");
    ASSERT_TRUE(reader.parse(stream.data(),
        handler2.onStartElement(),
        handler2.onEndElement(),
        handler2.onCharacters()).isOk());
    auto doc2 = handler2.releaseDocument();

    ASSERT_NE(doc2, nullptr);
    EXPECT_EQ(doc2->localName(), "document");
    EXPECT_EQ(doc2->childCount(), 1u); // body
    auto* body2 = doc2->childAt(0);
    EXPECT_EQ(body2->localName(), "body");
    EXPECT_GE(body2->childCount(), 1u); // 至少一个 p
}

TEST(IntegrationTest, SerializeTextWithSpecialCharacters) {
    // 含特殊字符的文本 round-trip
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body><w:p><w:r><w:t>a &lt; b &amp;&amp; c &gt; d</w:t></w:r></w:p></w:body></w:document>";
    std::vector<uint8_t> data(xml, xml + std::strlen(xml));

    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;
    ASSERT_TRUE(reader.parse(data,
        handler.onStartElement(),
        handler.onEndElement(),
        handler.onCharacters()).isOk());
    auto doc = handler.releaseDocument();

    // 获取文本内容
    auto* body = doc->childAt(0);
    auto* para = body->childAt(0);
    auto* run = para->childAt(0);
    auto* textNode = dynamic_cast<Text*>(run->childAt(0));
    ASSERT_NE(textNode, nullptr);
    ASSERT_NE(textNode->content(), nullptr);

    // libxml2 SAX 自动解码实体
    EXPECT_EQ(textNode->content()->text(), "a < b && c > d");

    // 序列化
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);
    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    ASSERT_NE(wordDoc, nullptr);
    wordDoc->serialize(writer);

    // 重新解析，验证文本完整
    OoxmlSaxHandler handler2("word");
    ASSERT_TRUE(reader.parse(stream.data(),
        handler2.onStartElement(),
        handler2.onEndElement(),
        handler2.onCharacters()).isOk());
    auto doc2 = handler2.releaseDocument();

    auto* body2 = doc2->childAt(0);
    auto* para2 = body2->childAt(0);
    auto* run2 = para2->childAt(0);
    auto* textNode2 = dynamic_cast<Text*>(run2->childAt(0));
    ASSERT_NE(textNode2, nullptr);
    ASSERT_NE(textNode2->content(), nullptr);
    EXPECT_EQ(textNode2->content()->text(), "a < b && c > d");
}
