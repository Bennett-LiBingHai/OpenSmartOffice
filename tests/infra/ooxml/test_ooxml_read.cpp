#include <gtest/gtest.h>
#include "oso/ooxml/read/IOoxmlReader.h"
#include "oso/ooxml/read/Libxml2Reader.h"
#include "oso/ooxml/common/OoxmlNamespaces.h"
#include "oso/ooxml/common/IZipArchive.h"
#include "oso/ooxml/common/LibzipZipArchive.h"
#include "oso/io/IStream.h"
#include "oso/base/ErrorCode.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include<cstring>

using namespace oso;
namespace fs = std::filesystem;

namespace {

struct TempFile {
    std::string path;
    explicit TempFile(std::string p) : path(std::move(p)) {}
    ~TempFile() { std::remove(path.c_str()); }
};

} // anonymous namespace

// ============================================================
// 基本 XML 解析
// ============================================================

TEST(Libxml2Reader, ParseSimpleXml) {
    Libxml2Reader reader;

    const char* xml = "<?xml version=\"1.0\"?><root><child>text</child></root>";
    std::vector<uint8_t> data(reinterpret_cast<const uint8_t*>(xml), reinterpret_cast<const uint8_t*>(xml) + std::strlen(xml));

    std::vector<std::string> events;

    auto result = reader.parse(data,
        [&](const std::string& nsUri, const std::string& localName,
            const std::string& qName, const std::vector<XmlAttribute>& attrs) {
            std::ostringstream oss;
            oss << "START ns='" << nsUri << "' local='" << localName
                << "' qName='" << qName << "' attrs=" << attrs.size();
            events.push_back(oss.str());
        },
        [&](const std::string& nsUri, const std::string& localName,
            const std::string& qName) {
            std::ostringstream oss;
            oss << "END ns='" << nsUri << "' local='" << localName
                << "' qName='" << qName << "'";
            events.push_back(oss.str());
        },
        [&](const std::string& text) {
            events.push_back("TEXT '" + text + "'");
        });

    ASSERT_TRUE(result.isOk()) << result.message();
    ASSERT_GE(events.size(), 3u);

    EXPECT_EQ(events[0], "START ns='' local='root' qName='root' attrs=0");
    EXPECT_EQ(events[1], "START ns='' local='child' qName='child' attrs=0");
    EXPECT_EQ(events[2], "TEXT 'text'");
    EXPECT_EQ(events[3], "END ns='' local='child' qName='child'");
    EXPECT_EQ(events[4], "END ns='' local='root' qName='root'");
}

// ============================================================
// 命名空间解析（SAX2 自动分解 nsUri + localName）
// ============================================================

TEST(Libxml2Reader, ParseNamespacedXml) {
    Libxml2Reader reader;

    const char* xml =
        "<?xml version=\"1.0\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body><w:p><w:r><w:t>Hello</w:t></w:r></w:p></w:body>"
        "</w:document>";
    std::vector<uint8_t> data(xml, xml + std::strlen(xml));

    std::vector<std::string> events;

    auto result = reader.parse(data,
        [&](const std::string& nsUri, const std::string& localName,
            const std::string& qName, const std::vector<XmlAttribute>&) {
            std::ostringstream oss;
            oss << "START ns='" << nsUri << "' local='" << localName
                << "' qName='" << qName << "'";
            events.push_back(oss.str());
        },
        [&](const std::string& nsUri, const std::string& localName,
            const std::string& qName) {
            std::ostringstream oss;
            oss << "END ns='" << nsUri << "' local='" << localName << "'";
            events.push_back(oss.str());
        },
        [&](const std::string& text) {
            events.push_back("TEXT '" + text + "'");
        });

    ASSERT_TRUE(result.isOk()) << result.message();

    // 验证命名空间 URI 被正确解析
    EXPECT_EQ(events[0],
        "START ns='http://schemas.openxmlformats.org/wordprocessingml/2006/main' "
        "local='document' qName='w:document'");
    EXPECT_EQ(events[1],
        "START ns='http://schemas.openxmlformats.org/wordprocessingml/2006/main' "
        "local='body' qName='w:body'");
    EXPECT_EQ(events[2],
        "START ns='http://schemas.openxmlformats.org/wordprocessingml/2006/main' "
        "local='p' qName='w:p'");
    EXPECT_EQ(events[3],
        "START ns='http://schemas.openxmlformats.org/wordprocessingml/2006/main' "
        "local='r' qName='w:r'");
    EXPECT_EQ(events[4],
        "START ns='http://schemas.openxmlformats.org/wordprocessingml/2006/main' "
        "local='t' qName='w:t'");
    EXPECT_EQ(events[5], "TEXT 'Hello'");
}

// ============================================================
// 属性解析（含命名空间属性）
// ============================================================

TEST(Libxml2Reader, ParseAttributes) {
    Libxml2Reader reader;

    const char* xml =
        "<?xml version=\"1.0\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:p w:rsidR=\"00AB1234\" w:rsidRDefault=\"00CD5678\">"
        "<w:r><w:rPr><w:b w:val=\"true\"/></w:rPr></w:r></w:p></w:document>";
    std::vector<uint8_t> data(xml, xml + std::strlen(xml));

    std::vector<XmlAttribute> capturedAttrs;

    auto result = reader.parse(data,
        [&](const std::string&, const std::string& localName,
            const std::string&, const std::vector<XmlAttribute>& attrs) {
            if (localName == "p") capturedAttrs = attrs;
        },
        nullptr, nullptr);

    ASSERT_TRUE(result.isOk()) << result.message();
    ASSERT_EQ(capturedAttrs.size(), 2u);

    // w:rsidR
    EXPECT_EQ(capturedAttrs[0].localName, "rsidR");
    EXPECT_EQ(capturedAttrs[0].namespaceUri,
              "http://schemas.openxmlformats.org/wordprocessingml/2006/main");
    EXPECT_EQ(capturedAttrs[0].prefix, "w");
    EXPECT_EQ(capturedAttrs[0].value, "00AB1234");

    // w:rsidRDefault
    EXPECT_EQ(capturedAttrs[1].localName, "rsidRDefault");
    EXPECT_EQ(capturedAttrs[1].value, "00CD5678");
}

// ============================================================
// 解析真实的 word/document.xml
// ============================================================

TEST(Libxml2Reader, ParseRealDocumentXml) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto xmlData = archive.readEntry("word/document.xml");
    ASSERT_TRUE(xmlData.isOk());

    Libxml2Reader reader;
    int startCount = 0;
    int endCount = 0;
    int textCount = 0;
    std::string rootLocalName;

    auto result = reader.parse(xmlData.value(),
        [&](const std::string& nsUri, const std::string& localName,
            const std::string&, const std::vector<XmlAttribute>&) {
            startCount++;
            if (startCount == 1) {
                rootLocalName = localName;
                EXPECT_EQ(nsUri,
                    "http://schemas.openxmlformats.org/wordprocessingml/2006/main");
            }
        },
        [&](const std::string&, const std::string&, const std::string&) {
            endCount++;
        },
        [&](const std::string&) {
            textCount++;
        });

    ASSERT_TRUE(result.isOk()) << result.message();
    EXPECT_EQ(rootLocalName, "document");
    EXPECT_GE(startCount, 3); // document, body, p
    EXPECT_EQ(endCount, startCount); // 每个开始配一个结束
}

// ============================================================
// 解析 [Content_Types].xml
// ============================================================

TEST(Libxml2Reader, ParseContentTypesXml) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto xmlData = archive.readEntry("[Content_Types].xml");
    ASSERT_TRUE(xmlData.isOk());

    Libxml2Reader reader;

    struct ElementInfo {
        std::string nsUri;
        std::string localName;
    };
    std::vector<ElementInfo> elements;

    auto result = reader.parse(xmlData.value(),
        [&](const std::string& nsUri, const std::string& localName,
            const std::string&, const std::vector<XmlAttribute>&) {
            elements.push_back({nsUri, localName});
        },
        nullptr, nullptr);

    ASSERT_TRUE(result.isOk()) << result.message();
    ASSERT_GE(elements.size(), 1u);

    EXPECT_EQ(elements[0].localName, "Types");
    EXPECT_EQ(elements[0].nsUri,
        "http://schemas.openxmlformats.org/package/2006/content-types");

    // 至少有一个 Override 或 Default
    bool foundOverride = false;
    bool foundDefault = false;
    for (const auto& el : elements) {
        if (el.localName == "Override") foundOverride = true;
        if (el.localName == "Default") foundDefault = true;
    }
    EXPECT_TRUE(foundOverride || foundDefault);
}

// ============================================================
// 错误处理
// ============================================================

TEST(Libxml2Reader, ParseEmptyData) {
    Libxml2Reader reader;
    std::vector<uint8_t> empty;
    auto result = reader.parse(empty, nullptr, nullptr, nullptr);
    EXPECT_TRUE(result.isErr());
    EXPECT_EQ(result.error(), ErrorCode::OOXML_XmlParseError);
}

TEST(Libxml2Reader, ParseMalformedXml) {
    Libxml2Reader reader;
    const char* xml = "<?xml version=\"1.0\"?><root><unclosed></root>";
    std::vector<uint8_t> data(xml, xml + std::strlen(xml));
    auto result = reader.parse(data, nullptr, nullptr, nullptr);
    EXPECT_TRUE(result.isErr());
    EXPECT_EQ(result.error(), ErrorCode::OOXML_XmlParseError);
}

TEST(Libxml2Reader, ParseNoCallbacksOk) {
    Libxml2Reader reader;
    const char* xml = "<?xml version=\"1.0\"?><root><child/></root>";
    std::vector<uint8_t> data(xml, xml + std::strlen(xml));

    auto result = reader.parse(data, nullptr, nullptr, nullptr);
    EXPECT_TRUE(result.isOk()); // null callbacks are fine, just don't fire
}

// ============================================================
// parseStream() — 从 MemoryStream 解析
// ============================================================

TEST(Libxml2Reader, ParseStreamFromMemory) {
    const char* xml = "<?xml version=\"1.0\"?><root attr=\"val\"><child/></root>";
    std::vector<uint8_t> data(xml, xml + std::strlen(xml));
    MemoryStream stream(data);

    Libxml2Reader reader;
    std::vector<std::string> events;

    auto result = reader.parseStream(stream,
        [&](const std::string&, const std::string& localName,
            const std::string& qName, const std::vector<XmlAttribute>& attrs) {
            std::ostringstream oss;
            oss << "START local='" << localName << "' attrs=" << attrs.size();
            events.push_back(oss.str());
        },
        [&](const std::string&, const std::string& localName, const std::string&) {
            events.push_back("END local='" + localName + "'");
        },
        nullptr);

    ASSERT_TRUE(result.isOk()) << result.message();
    ASSERT_EQ(events.size(), 4u);
    EXPECT_EQ(events[0], "START local='root' attrs=1");
    EXPECT_EQ(events[1], "START local='child' attrs=0");
    EXPECT_EQ(events[2], "END local='child'");
    EXPECT_EQ(events[3], "END local='root'");
}

TEST(Libxml2Reader, ParseStreamWithText) {
    const char* xml = "<?xml version=\"1.0\"?><a>hello<b/>world</a>";
    std::vector<uint8_t> data(xml, xml + std::strlen(xml));
    MemoryStream stream(data);

    Libxml2Reader reader;
    std::vector<std::string> texts;

    auto result = reader.parseStream(stream,
        [&](const std::string&, const std::string& localName,
            const std::string&, const std::vector<XmlAttribute>&) {
            texts.push_back("START " + localName);
        },
        [&](const std::string&, const std::string& localName, const std::string&) {
            texts.push_back("END " + localName);
        },
        [&](const std::string& text) {
            texts.push_back("TEXT " + text);
        });

    ASSERT_TRUE(result.isOk()) << result.message();
    ASSERT_EQ(texts.size(), 6u);
    EXPECT_EQ(texts[0], "START a");
    EXPECT_EQ(texts[1], "TEXT hello");
    EXPECT_EQ(texts[2], "START b");
    EXPECT_EQ(texts[3], "END b");
    EXPECT_EQ(texts[4], "TEXT world");
    EXPECT_EQ(texts[5], "END a");
}

// ============================================================
// OoxmlNamespaces 常量验证
// ============================================================

TEST(OoxmlNamespaces, WordprocessingML) {
    EXPECT_EQ(OoxmlNamespaces::kWordprocessingML,
        "http://schemas.openxmlformats.org/wordprocessingml/2006/main");
}

TEST(OoxmlNamespaces, SpreadsheetML) {
    EXPECT_EQ(OoxmlNamespaces::kSpreadsheetML,
        "http://schemas.openxmlformats.org/spreadsheetml/2006/main");
}

TEST(OoxmlNamespaces, DrawingML) {
    EXPECT_EQ(OoxmlNamespaces::kDrawingML,
        "http://schemas.openxmlformats.org/drawingml/2006/main");
}

TEST(OoxmlNamespaces, PrefixMapContainsCorePrefixes) {
    const auto& map = OoxmlNamespaces::prefixMap();
    EXPECT_EQ(map.at("w"), OoxmlNamespaces::kWordprocessingML);
    EXPECT_EQ(map.at("x"), OoxmlNamespaces::kSpreadsheetML);
    EXPECT_EQ(map.at("p"), OoxmlNamespaces::kPresentationML);
    EXPECT_EQ(map.at("a"), OoxmlNamespaces::kDrawingML);
    EXPECT_EQ(map.at("r"), OoxmlNamespaces::kRelationships);
}
