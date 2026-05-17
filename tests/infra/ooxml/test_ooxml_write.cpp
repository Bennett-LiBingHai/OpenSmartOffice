#include "oso/io/IStream.h"
#include "oso/ooxml/common/OoxmlNamespaces.h"
#include "oso/ooxml/read/Libxml2Reader.h"
#include "oso/ooxml/write/IOoxmlWriter.h"
#include "oso/ooxml/write/Libxml2Writer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>

using namespace oso;

// ============================================================
// 辅助函数：通过 Libxml2Reader round-trip 验证 XML
// ============================================================
namespace {

struct RoundTripResult {
    std::vector<std::string> startElements;
    std::vector<std::string> texts;
    bool success = false;
};

RoundTripResult roundTrip(const std::vector<uint8_t>& xml) {
    RoundTripResult r;
    Libxml2Reader reader;
    std::string textBuf;

    auto flushText = [&]() {
        // 跳过纯空白（xmlTextWriter 缩进），但保留含内容的文本
        if (!textBuf.empty() && !std::all_of(textBuf.begin(), textBuf.end(),
                                             [](unsigned char c) { return std::isspace(c); })) {
            r.texts.push_back(textBuf);
        }
        textBuf.clear();
    };

    auto res = reader.parse(
        xml,
        [&](const std::string&, const std::string& localName, const std::string&,
            const std::vector<XmlAttribute>&) {
            flushText();
            r.startElements.push_back(localName);
        },
        [&](const std::string&, const std::string&, const std::string&) { flushText(); },
        [&](const std::string& text) {
            // SAX 可能将文本拆成多个回调，拼合到缓冲区
            textBuf += text;
        });
    flushText();
    r.success = res.isOk();
    return r;
}

}  // anonymous namespace

// ============================================================
// 基本元素写出与 round-trip
// ============================================================

TEST(Libxml2Writer, WriteSimpleDocument) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    ASSERT_TRUE(writer.writeStartDocument().isOk());
    ASSERT_TRUE(writer.writeStartElement("", "root").isOk());
    ASSERT_TRUE(writer.writeStartElement("", "child").isOk());
    ASSERT_TRUE(writer.writeText("hello").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    auto result = roundTrip(stream.data());
    ASSERT_TRUE(result.success);
    ASSERT_GE(result.startElements.size(), 2u);
    EXPECT_EQ(result.startElements[0], "root");
    EXPECT_EQ(result.startElements[1], "child");
    ASSERT_GE(result.texts.size(), 1u);
    EXPECT_EQ(result.texts[0], "hello");
}

// ============================================================
// XML 声明
// ============================================================

TEST(Libxml2Writer, WriteDeclaration) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    ASSERT_TRUE(writer.writeStartDocument(true).isOk());
    ASSERT_TRUE(writer.writeStartElement("", "r").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    std::string xmlStr(reinterpret_cast<const char*>(stream.data().data()), stream.data().size());
    EXPECT_NE(xmlStr.find("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"),
              std::string::npos);
}

TEST(Libxml2Writer, WriteDeclarationNoStandalone) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    ASSERT_TRUE(writer.writeStartDocument(false).isOk());
    ASSERT_TRUE(writer.writeStartElement("", "r").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    std::string xmlStr(reinterpret_cast<const char*>(stream.data().data()), stream.data().size());
    EXPECT_NE(xmlStr.find("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"), std::string::npos);
    EXPECT_EQ(xmlStr.find("standalone"), std::string::npos);
}

// ============================================================
// 命名空间元素写出
// ============================================================

TEST(Libxml2Writer, WriteNamespacedDocument) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    const auto& ns = ooxml_namespaces::kWordprocessingML;
    ASSERT_TRUE(writer.writeStartDocument().isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(ns), "document").isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(ns), "body").isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(ns), "p").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    auto result = roundTrip(stream.data());
    ASSERT_TRUE(result.success);
    ASSERT_GE(result.startElements.size(), 3u);
    EXPECT_EQ(result.startElements[0], "document");
    EXPECT_EQ(result.startElements[1], "body");
    EXPECT_EQ(result.startElements[2], "p");

    // xmlTextWriter 应自动声明 w 前缀
    std::string xmlStr(reinterpret_cast<const char*>(stream.data().data()), stream.data().size());
    EXPECT_NE(xmlStr.find("xmlns:w="), std::string::npos);
}

// ============================================================
// 属性写出
// ============================================================

TEST(Libxml2Writer, WriteAttributes) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    const auto& ns = ooxml_namespaces::kWordprocessingML;
    ASSERT_TRUE(writer.writeStartDocument().isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(ns), "p").isOk());
    ASSERT_TRUE(writer.writeAttribute(std::string(ns), "rsidR", "00AB1234").isOk());
    ASSERT_TRUE(writer.writeAttribute(std::string(ns), "rsidRDefault", "00CD5678").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    // Round-trip 验证属性
    Libxml2Reader reader;
    std::vector<XmlAttribute> capturedAttrs;

    auto res = reader.parse(
        stream.data(),
        [&](const std::string&, const std::string& localName, const std::string&,
            const std::vector<XmlAttribute>& attrs) {
            if (localName == "p") capturedAttrs = attrs;
        },
        nullptr, nullptr);

    ASSERT_TRUE(res.isOk());
    ASSERT_EQ(capturedAttrs.size(), 2u);
    EXPECT_EQ(capturedAttrs[0].localName, "rsidR");
    EXPECT_EQ(capturedAttrs[0].value, "00AB1234");
    EXPECT_EQ(capturedAttrs[1].localName, "rsidRDefault");
    EXPECT_EQ(capturedAttrs[1].value, "00CD5678");
}

// ============================================================
// XML 转义（通过 round-trip 验证）
// ============================================================

TEST(Libxml2Writer, WriteEscapedText) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    ASSERT_TRUE(writer.writeStartDocument().isOk());
    ASSERT_TRUE(writer.writeStartElement("", "root").isOk());
    ASSERT_TRUE(writer.writeText("a < b && c > \"d\" 'e'").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    // Round-trip 应还原原始文本
    auto result = roundTrip(stream.data());
    ASSERT_TRUE(result.success);
    ASSERT_GE(result.texts.size(), 1u);
    EXPECT_EQ(result.texts[0], "a < b && c > \"d\" 'e'");
}

TEST(Libxml2Writer, WriteEmptyText) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    ASSERT_TRUE(writer.writeStartDocument().isOk());
    ASSERT_TRUE(writer.writeStartElement("", "root").isOk());
    ASSERT_TRUE(writer.writeText("").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    auto result = roundTrip(stream.data());
    ASSERT_TRUE(result.success);
}

// ============================================================
// 嵌套元素
// ============================================================

TEST(Libxml2Writer, WriteNestedElements) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    ASSERT_TRUE(writer.writeStartDocument().isOk());
    ASSERT_TRUE(writer.writeStartElement("", "a").isOk());
    ASSERT_TRUE(writer.writeStartElement("", "b").isOk());
    ASSERT_TRUE(writer.writeStartElement("", "c").isOk());
    ASSERT_TRUE(writer.writeText("deep").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());  // c
    ASSERT_TRUE(writer.writeEndElement().isOk());  // b
    ASSERT_TRUE(writer.writeEndElement().isOk());  // a
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    auto result = roundTrip(stream.data());
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.startElements.size(), 3u);
    EXPECT_EQ(result.startElements[0], "a");
    EXPECT_EQ(result.startElements[1], "b");
    EXPECT_EQ(result.startElements[2], "c");
    ASSERT_GE(result.texts.size(), 1u);
    for (int i = 0; i < result.texts.size(); ++i) std::cout << result.texts[i] << std::endl;
    EXPECT_EQ(result.texts[0], "deep");
}

// ============================================================
// 命名空间前缀使用正确性
// ============================================================

TEST(Libxml2Writer, WriteSpreadsheetML) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    const auto& ns = ooxml_namespaces::kSpreadsheetML;
    ASSERT_TRUE(writer.writeStartDocument().isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(ns), "worksheet").isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(ns), "sheetData").isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(ns), "row").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    auto result = roundTrip(stream.data());
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.startElements[0], "worksheet");
    EXPECT_EQ(result.startElements[1], "sheetData");
    EXPECT_EQ(result.startElements[2], "row");

    std::string xmlStr(reinterpret_cast<const char*>(stream.data().data()), stream.data().size());
    EXPECT_NE(xmlStr.find("xmlns:x="), std::string::npos);
}

TEST(Libxml2Writer, WritePresentationML) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    const auto& ns = ooxml_namespaces::kPresentationML;
    ASSERT_TRUE(writer.writeStartDocument().isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(ns), "presentation").isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(ns), "sldIdLst").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    auto result = roundTrip(stream.data());
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.startElements[0], "presentation");
    EXPECT_EQ(result.startElements[1], "sldIdLst");

    std::string xmlStr(reinterpret_cast<const char*>(stream.data().data()), stream.data().size());
    EXPECT_NE(xmlStr.find("xmlns:p="), std::string::npos);
}

// ============================================================
// 多命名空间文档
// ============================================================

TEST(Libxml2Writer, WriteMultipleNamespaces) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    const auto& wns = ooxml_namespaces::kWordprocessingML;
    const auto& rns = ooxml_namespaces::kRelationships;

    ASSERT_TRUE(writer.writeStartDocument().isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(wns), "document").isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(wns), "body").isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(wns), "p").isOk());
    ASSERT_TRUE(writer.writeStartElement(std::string(wns), "r").isOk());
    ASSERT_TRUE(writer.writeAttribute(std::string(rns), "id", "rId1").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    auto result = roundTrip(stream.data());
    ASSERT_TRUE(result.success);

    std::string xmlStr(reinterpret_cast<const char*>(stream.data().data()), stream.data().size());
    std::cout << xmlStr << std::endl;
    EXPECT_NE(xmlStr.find("xmlns:w="), std::string::npos);
    EXPECT_NE(xmlStr.find("xmlns:r="), std::string::npos);
}

// ============================================================
// 无命名空间属性
// ============================================================

TEST(Libxml2Writer, WriteAttributeNoNamespace) {
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    ASSERT_TRUE(writer.writeStartDocument().isOk());
    ASSERT_TRUE(writer.writeStartElement("", "root").isOk());
    ASSERT_TRUE(writer.writeAttribute("", "id", "123").isOk());
    ASSERT_TRUE(writer.writeText("content").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    Libxml2Reader reader;
    std::vector<XmlAttribute> capturedAttrs;

    auto res = reader.parse(
        stream.data(),
        [&](const std::string&, const std::string& localName, const std::string&,
            const std::vector<XmlAttribute>& attrs) {
            if (localName == "root") capturedAttrs = attrs;
        },
        nullptr, nullptr);

    ASSERT_TRUE(res.isOk());
    ASSERT_EQ(capturedAttrs.size(), 1u);
    EXPECT_EQ(capturedAttrs[0].localName, "id");
    EXPECT_EQ(capturedAttrs[0].value, "123");
}

// ============================================================
// xmllint well-formed 验证（如果 xmllint 可用）
// ============================================================

TEST(Libxml2Writer, XmllintValidation) {
    // 检查 xmllint 是否可用
    if (std::system("which xmllint >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "xmllint not available";
    }

    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);

    ASSERT_TRUE(writer.writeStartDocument().isOk());
    ASSERT_TRUE(writer.writeStartElement("", "root").isOk());
    ASSERT_TRUE(writer.writeAttribute("", "version", "1").isOk());
    ASSERT_TRUE(writer.writeStartElement("", "child").isOk());
    ASSERT_TRUE(writer.writeAttribute("", "flag", "true").isOk());
    ASSERT_TRUE(writer.writeText("data").isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndElement().isOk());
    ASSERT_TRUE(writer.writeEndDocument().isOk());

    std::string tmpPath = "/tmp/oso_write_test_" +
                          std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
                          ".xml";
    {
        auto* fd = std::fopen(tmpPath.c_str(), "wb");
        ASSERT_NE(fd, nullptr);
        std::fwrite(stream.data().data(), 1, stream.data().size(), fd);
        std::fclose(fd);
    }

    std::string cmd = "xmllint --noout \"" + tmpPath + "\" 2>&1";
    int xmllintRc = std::system(cmd.c_str());
    EXPECT_EQ(xmllintRc, 0) << "xmllint rejected the output XML";

    std::remove(tmpPath.c_str());
}

// ============================================================
// 错误处理：未设置输出时调用 write
// ============================================================

TEST(Libxml2Writer, WriteWithoutOutput) {
    Libxml2Writer writer;
    auto result = writer.writeStartDocument();
    EXPECT_TRUE(result.isErr());
    EXPECT_EQ(result.error(), ErrorCode::OOXMLXmlParseError);
}
