#include "oso/base/ErrorCode.h"
#include "oso/ooxml/common/ContentTypeRegistry.h"
#include "oso/ooxml/common/IZipArchive.h"
#include "oso/ooxml/common/LibzipZipArchive.h"
#include "oso/ooxml/common/RelationshipMap.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>

using namespace oso;
namespace fs = std::filesystem;

namespace {

// RAII temp file cleanup
struct TempFile {
    std::string path;
    bool isRemain;
    explicit TempFile(std::string p, bool ir = false) : path(std::move(p)), isRemain(ir) {
    }
    ~TempFile() {
        if (!isRemain)
            std::remove(path.c_str());
    }
};

std::string tempPath(const std::string& name) {
    return (fs::temp_directory_path() / name).string();
}

}  // anonymous namespace

// ============================================================
// LibzipZipArchive
// ============================================================

TEST(LibzipZipArchive, OpenNonExistent) {
    LibzipZipArchive archive;
    auto result = archive.open("nonexistent_file.docx");
    EXPECT_TRUE(result.isErr());
}

TEST(LibzipZipArchive, OpenInvalidFile) {
    LibzipZipArchive archive;
    auto result = archive.open(TEST_FIXTURE_DIR "/nonexistent.docx");
    EXPECT_TRUE(result.isErr());
}

TEST(LibzipZipArchive, OpenValidDocx) {
    LibzipZipArchive archive;
    auto result = archive.open(TEST_FIXTURE_DIR "/minimal.docx");
    EXPECT_TRUE(result.isOk()) << result.message();
    EXPECT_TRUE(archive.isOpen());
}

TEST(LibzipZipArchive, CloseArchive) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());
    auto result = archive.close();
    EXPECT_TRUE(result.isOk());
    EXPECT_FALSE(archive.isOpen());
}

TEST(LibzipZipArchive, EntriesList) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto entries = archive.entries();
    ASSERT_TRUE(entries.isOk()) << entries.message();

    auto& list = entries.value();
    EXPECT_GE(list.size(), 3u);  // At least [Content_Types].xml, _rels/.rels, word/document.xml

    // Check that [Content_Types].xml is present
    auto hasContentTypes = std::find_if(list.begin(), list.end(), [](const IZipArchive::Entry& e) {
        return e.name == "[Content_Types].xml";
    });
    EXPECT_NE(hasContentTypes, list.end());

    // Check that word/document.xml is present
    auto hasDocument = std::find_if(list.begin(), list.end(), [](const IZipArchive::Entry& e) {
        return e.name == "word/document.xml";
    });
    EXPECT_NE(hasDocument, list.end());
}

TEST(LibzipZipArchive, ReadEntry) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto data = archive.readEntry("[Content_Types].xml");
    ASSERT_TRUE(data.isOk()) << data.message();
    EXPECT_GT(data.value().size(), 0u);

    // Verify it's XML by checking first bytes
    std::string content(reinterpret_cast<const char*>(data.value().data()), data.value().size());
    EXPECT_TRUE(content.find("<?xml") != std::string::npos ||
                content.find("<Types") != std::string::npos);
}

TEST(LibzipZipArchive, ReadMissingEntry) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto data = archive.readEntry("nonexistent_part.xml");
    EXPECT_TRUE(data.isErr());
    EXPECT_EQ(data.error(), ErrorCode::OOXML_ZipEntryMissing);
}

// ============================================================
// ContentTypeRegistry
// ============================================================

TEST(ContentTypeRegistry, ParseValid) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto xmlData = archive.readEntry("[Content_Types].xml");
    ASSERT_TRUE(xmlData.isOk());

    auto result = ContentTypeRegistry::parse(xmlData.value());
    ASSERT_TRUE(result.isOk()) << result.message();

    auto& registry = result.value();
    EXPECT_GE(registry.allTypes().size(), 2u);
}

TEST(ContentTypeRegistry, GetTypeForPartByOverride) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto xmlData = archive.readEntry("[Content_Types].xml");
    ASSERT_TRUE(xmlData.isOk());

    auto ctResult = ContentTypeRegistry::parse(xmlData.value());
    ASSERT_TRUE(ctResult.isOk());

    auto type = ctResult.value().getTypeForPart("word/document.xml");
    ASSERT_TRUE(type.isOk()) << type.message();
    EXPECT_EQ(type.value(),
              "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml");
}

TEST(ContentTypeRegistry, GetTypeForExtension) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto xmlData = archive.readEntry("[Content_Types].xml");
    ASSERT_TRUE(xmlData.isOk());

    auto ctResult = ContentTypeRegistry::parse(xmlData.value());
    ASSERT_TRUE(ctResult.isOk());

    auto type = ctResult.value().getTypeForExtension("xml");
    ASSERT_TRUE(type.isOk()) << type.message();
    EXPECT_EQ(type.value(), "application/xml");
}

TEST(ContentTypeRegistry, GetTypeForPartFallsBackToExtension) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto xmlData = archive.readEntry("[Content_Types].xml");
    ASSERT_TRUE(xmlData.isOk());

    auto ctResult = ContentTypeRegistry::parse(xmlData.value());
    ASSERT_TRUE(ctResult.isOk());

    // A part that has no Override but has a .xml extension
    auto type = ctResult.value().getTypeForPart("some/other/file.xml");
    ASSERT_TRUE(type.isOk()) << type.message();
    EXPECT_EQ(type.value(), "application/xml");
}

TEST(ContentTypeRegistry, UnknownPartReturnsError) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto xmlData = archive.readEntry("[Content_Types].xml");
    ASSERT_TRUE(xmlData.isOk());

    auto ctResult = ContentTypeRegistry::parse(xmlData.value());
    ASSERT_TRUE(ctResult.isOk());

    auto type = ctResult.value().getTypeForPart("unknown.bin");
    EXPECT_TRUE(type.isErr());
}

TEST(ContentTypeRegistry, EmptyData) {
    std::vector<uint8_t> empty;
    auto result = ContentTypeRegistry::parse(empty);
    EXPECT_TRUE(result.isErr());
    EXPECT_EQ(result.error(), ErrorCode::OOXML_XmlParseError);
}

// ============================================================
// RelationshipMap
// ============================================================

TEST(RelationshipMap, ParseValid) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto xmlData = archive.readEntry("_rels/.rels");
    ASSERT_TRUE(xmlData.isOk());

    auto result = RelationshipMap::parse(xmlData.value());
    ASSERT_TRUE(result.isOk()) << result.message();

    auto& map = result.value();
    EXPECT_GE(map.getAll().size(), 1u);
}

TEST(RelationshipMap, GetById) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto xmlData = archive.readEntry("_rels/.rels");
    ASSERT_TRUE(xmlData.isOk());

    auto result = RelationshipMap::parse(xmlData.value());
    ASSERT_TRUE(result.isOk());

    auto rel = result.value().getById("rId1");
    ASSERT_TRUE(rel.isOk()) << rel.message();
    EXPECT_EQ(rel.value()->id, "rId1");
    EXPECT_EQ(rel.value()->target, "word/document.xml");
    EXPECT_FALSE(rel.value()->isExternal);
}

TEST(RelationshipMap, GetByTarget) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto xmlData = archive.readEntry("_rels/.rels");
    ASSERT_TRUE(xmlData.isOk());

    auto result = RelationshipMap::parse(xmlData.value());
    ASSERT_TRUE(result.isOk());

    auto rel = result.value().getByTarget("word/document.xml");
    ASSERT_TRUE(rel.isOk()) << rel.message();
    EXPECT_EQ(rel.value()->id, "rId1");
}

TEST(RelationshipMap, MissingId) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto xmlData = archive.readEntry("_rels/.rels");
    ASSERT_TRUE(xmlData.isOk());

    auto result = RelationshipMap::parse(xmlData.value());
    ASSERT_TRUE(result.isOk());

    auto rel = result.value().getById("rId999");
    EXPECT_TRUE(rel.isErr());
}

TEST(RelationshipMap, EmptyData) {
    std::vector<uint8_t> empty;
    auto result = RelationshipMap::parse(empty);
    EXPECT_TRUE(result.isErr());
}

// ============================================================
// Integration: full open-and-list workflow
// ============================================================

TEST(IntegrationTest, OpenDocxAndListPartsWithMimeTypes) {
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(TEST_FIXTURE_DIR "/minimal.docx").isOk());

    auto entries = archive.entries();
    ASSERT_TRUE(entries.isOk());

    auto xmlData = archive.readEntry("[Content_Types].xml");
    ASSERT_TRUE(xmlData.isOk());

    auto ctResult = ContentTypeRegistry::parse(xmlData.value());
    ASSERT_TRUE(ctResult.isOk());

    // For each entry, try to get its MIME type
    for (const auto& entry : entries.value()) {
        if (entry.isDirectory)
            continue;
        auto mimeType = ctResult.value().getTypeForPart(entry.name);
        // All parts should have a MIME type
        EXPECT_TRUE(mimeType.isOk())
            << "Entry '" << entry.name << "' has no MIME type: " << mimeType.message();
    }
}

// ============================================================
// M1.5 — Zip write tests
// ============================================================

TEST(LibzipZipArchive, CreateAndClose) {
    TempFile tmp(tempPath("test_create_empty.docx"));
    LibzipZipArchive archive;

    auto result = archive.create(tmp.path);
    ASSERT_TRUE(result.isOk()) << result.message();
    EXPECT_TRUE(archive.isOpen());

    result = archive.close();
    ASSERT_TRUE(result.isOk()) << result.message();
    EXPECT_FALSE(archive.isOpen());
}

TEST(LibzipZipArchive, WriteEntry) {
    TempFile tmp(tempPath("test_write_entry.docx"));
    LibzipZipArchive archive;

    ASSERT_TRUE(archive.create(tmp.path).isOk());

    const std::string content = "<?xml version=\"1.0\"?><root>hello</root>";
    std::vector<uint8_t> data(content.begin(), content.end());
    auto result = archive.writeEntry("test.xml", data);
    ASSERT_TRUE(result.isOk()) << result.message();

    ASSERT_TRUE(archive.close().isOk());

    // Read back and verify
    LibzipZipArchive reader;
    ASSERT_TRUE(reader.open(tmp.path).isOk());
    auto readBack = reader.readEntry("test.xml");
    ASSERT_TRUE(readBack.isOk());
    EXPECT_EQ(readBack.value(), data);
}

TEST(LibzipZipArchive, WriteMultipleEntries) {
    TempFile tmp(tempPath("test_write_multi.docx"));
    LibzipZipArchive archive;

    ASSERT_TRUE(archive.create(tmp.path).isOk());

    std::vector<uint8_t> data1 = {0x00, 0x01, 0x02, 0x03};
    std::vector<uint8_t> data2 = {0xFF, 0xFE, 0xFD};
    std::string text = "plain text content";
    std::vector<uint8_t> data3(text.begin(), text.end());

    ASSERT_TRUE(archive.writeEntry("binary.bin", data1).isOk());
    ASSERT_TRUE(archive.writeEntry("other.bin", data2).isOk());
    ASSERT_TRUE(archive.writeEntry("text.txt", data3).isOk());
    ASSERT_TRUE(archive.close().isOk());

    // Read back all
    LibzipZipArchive reader;
    ASSERT_TRUE(reader.open(tmp.path).isOk());

    auto r1 = reader.readEntry("binary.bin");
    ASSERT_TRUE(r1.isOk());
    EXPECT_EQ(r1.value(), data1);

    auto r2 = reader.readEntry("other.bin");
    ASSERT_TRUE(r2.isOk());
    EXPECT_EQ(r2.value(), data2);

    auto r3 = reader.readEntry("text.txt");
    ASSERT_TRUE(r3.isOk());
    EXPECT_EQ(r3.value(), data3);

    // Check entries list
    auto entries = reader.entries();
    ASSERT_TRUE(entries.isOk());
    EXPECT_EQ(entries.value().size(), 3u);
}

TEST(LibzipZipArchive, Overwrite) {
    TempFile tmp(tempPath("test_overwrite.docx"));
    LibzipZipArchive archive;

    // First write
    ASSERT_TRUE(archive.create(tmp.path).isOk());
    std::vector<uint8_t> oldData = {0x01, 0x02};
    ASSERT_TRUE(archive.writeEntry("data.bin", oldData).isOk());
    ASSERT_TRUE(archive.close().isOk());

    // Second write (overwrite)
    ASSERT_TRUE(archive.create(tmp.path).isOk());
    std::vector<uint8_t> newData = {0xAA, 0xBB, 0xCC, 0xDD};
    ASSERT_TRUE(archive.writeEntry("data.bin", newData).isOk());
    ASSERT_TRUE(archive.close().isOk());

    // Read back - should get new data
    LibzipZipArchive reader;
    ASSERT_TRUE(reader.open(tmp.path).isOk());
    auto rb = reader.readEntry("data.bin");
    ASSERT_TRUE(rb.isOk());
    EXPECT_EQ(rb.value(), newData);
}

TEST(LibzipZipArchive, CreateClosesPrevious) {
    TempFile tmp1(tempPath("test_prev1.docx"));
    TempFile tmp2(tempPath("test_prev2.docx"));
    LibzipZipArchive archive;

    // Create first, don't close
    ASSERT_TRUE(archive.create(tmp1.path).isOk());
    ASSERT_TRUE(archive.writeEntry("a.xml", std::vector<uint8_t>{0x01}).isOk());

    // Create second without closing first - should auto-close previous
    ASSERT_TRUE(archive.create(tmp2.path).isOk());
    ASSERT_TRUE(archive.writeEntry("b.xml", std::vector<uint8_t>{0x02}).isOk());
    ASSERT_TRUE(archive.close().isOk());

    // tmp2 should have b.xml
    LibzipZipArchive reader;
    ASSERT_TRUE(reader.open(tmp2.path).isOk());
    auto entries = reader.entries();
    ASSERT_TRUE(entries.isOk());
    EXPECT_EQ(entries.value().size(), 1u);
    EXPECT_EQ(entries.value()[0].name, "b.xml");
}

TEST(LibzipZipArchive, WriteWithoutCreate) {
    LibzipZipArchive archive;
    std::vector<uint8_t> data = {0x01};
    auto result = archive.writeEntry("test.xml", data);
    EXPECT_TRUE(result.isErr());
    EXPECT_EQ(result.error(), ErrorCode::OOXML_ZipWriteError);
}

// ============================================================
// ContentTypeRegistry::generate tests
// ============================================================

TEST(ContentTypeRegistry, GenerateRoundTrip) {
    auto xmlData = ContentTypeRegistry::generate({"word/document.xml"});
    EXPECT_GT(xmlData.size(), 0u);

    // Parse back
    auto result = ContentTypeRegistry::parse(xmlData);
    ASSERT_TRUE(result.isOk()) << result.message();

    // Should find the override for word/document.xml
    auto type = result.value().getTypeForPart("word/document.xml");
    ASSERT_TRUE(type.isOk());
    EXPECT_EQ(type.value(),
              "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml");

    // _rels/.rels should resolve via "rels" Default
    auto typeRels = result.value().getTypeForPart("_rels/.rels");
    ASSERT_TRUE(typeRels.isOk());
    EXPECT_EQ(typeRels.value(), "application/vnd.openxmlformats-package.relationships+xml");
}

TEST(ContentTypeRegistry, GenerateIncludesDefaults) {
    auto xmlData = ContentTypeRegistry::generate({
        "word/document.xml",
        "docProps/core.xml",
    });

    auto result = ContentTypeRegistry::parse(xmlData);
    ASSERT_TRUE(result.isOk());

    auto t1 = result.value().getTypeForPart("word/document.xml");
    ASSERT_TRUE(t1.isOk());

    auto t2 = result.value().getTypeForPart("docProps/core.xml");
    ASSERT_TRUE(t2.isOk());
}

TEST(ContentTypeRegistry, GenerateWithImageParts) {
    auto xmlData = ContentTypeRegistry::generate({"word/media/image1.png"});

    auto result = ContentTypeRegistry::parse(xmlData);
    ASSERT_TRUE(result.isOk());

    // .png extension should resolve via Default
    auto type = result.value().getTypeForPart("word/media/image1.png");
    ASSERT_TRUE(type.isOk());
    EXPECT_EQ(type.value(), "image/png");
}

TEST(ContentTypeRegistry, GeneratePrefixMatch) {
    // Sheet numbering: xl/worksheets/sheet1.xml, sheet2.xml should all match
    auto xmlData = ContentTypeRegistry::generate({
        "xl/workbook.xml",
        "xl/worksheets/sheet1.xml",
        "xl/worksheets/sheet2.xml",
    });

    auto result = ContentTypeRegistry::parse(xmlData);
    ASSERT_TRUE(result.isOk());

    auto t1 = result.value().getTypeForPart("xl/worksheets/sheet1.xml");
    ASSERT_TRUE(t1.isOk());
    EXPECT_EQ(t1.value(),
              "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml");

    auto t2 = result.value().getTypeForPart("xl/worksheets/sheet2.xml");
    ASSERT_TRUE(t2.isOk());
    EXPECT_EQ(t2.value(), t1.value());
}

// ============================================================
// RelationshipMap::generate tests
// ============================================================

TEST(RelationshipMap, GenerateRoundTrip) {
    std::vector<RelationshipMap::Relationship> rels = {
        {"rId1",
         "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument",
         "word/document.xml", false},
    };

    auto xmlData = RelationshipMap::generate(rels);
    EXPECT_GT(xmlData.size(), 0u);

    auto result = RelationshipMap::parse(xmlData);
    ASSERT_TRUE(result.isOk()) << result.message();

    auto rel = result.value().getById("rId1");
    ASSERT_TRUE(rel.isOk());
    EXPECT_EQ(rel.value()->id, "rId1");
    EXPECT_EQ(rel.value()->target, "word/document.xml");
}

TEST(RelationshipMap, GenerateExternal) {
    std::vector<RelationshipMap::Relationship> rels = {
        {"rId1", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink",
         "https://example.com", true},
    };

    auto xmlData = RelationshipMap::generate(rels);

    auto result = RelationshipMap::parse(xmlData);
    ASSERT_TRUE(result.isOk());

    auto rel = result.value().getById("rId1");
    ASSERT_TRUE(rel.isOk());
    EXPECT_TRUE(rel.value()->isExternal);
    EXPECT_EQ(rel.value()->target, "https://example.com");
}

// ============================================================
// Integration: blank docx round-trip
// ============================================================

// OOXML MIME types
static const char* kDocxDocument =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml";
static const char* kOfficeDocRel =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument";

static std::vector<uint8_t> makeMinimalDocumentXml() {
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body><w:p/></w:body></w:document>";
    std::string s(xml);
    return std::vector<uint8_t>(s.begin(), s.end());
}

TEST(IntegrationTest, BlankDocxRoundTrip) {
    TempFile tmp(tempPath("test_blank_roundtrip.docx"));

    // === Write phase ===
    LibzipZipArchive writer;

    // 1. Write [Content_Types].xml first (OOXML spec requirement)
    auto ctXml = ContentTypeRegistry::generate({"word/document.xml"});
    ASSERT_TRUE(writer.create(tmp.path).isOk());
    ASSERT_TRUE(writer.writeEntry("[Content_Types].xml", ctXml).isOk());

    // 2. Write _rels/.rels second
    auto relsXml = RelationshipMap::generate({
        {"rId1", kOfficeDocRel, "word/document.xml", false},
    });
    ASSERT_TRUE(writer.writeEntry("_rels/.rels", relsXml).isOk());

    // 3. Write word/document.xml last
    ASSERT_TRUE(writer.writeEntry("word/document.xml", makeMinimalDocumentXml()).isOk());

    ASSERT_TRUE(writer.close().isOk());

    // === Read-back phase ===
    LibzipZipArchive reader;
    ASSERT_TRUE(reader.open(tmp.path).isOk());

    // Verify all parts present
    auto entries = reader.entries();
    ASSERT_TRUE(entries.isOk());
    ASSERT_GE(entries.value().size(), 3u);

    // Verify [Content_Types].xml exists and parses
    auto ctBack = reader.readEntry("[Content_Types].xml");
    ASSERT_TRUE(ctBack.isOk());
    auto ctParsed = ContentTypeRegistry::parse(ctBack.value());
    ASSERT_TRUE(ctParsed.isOk());
    auto docType = ctParsed.value().getTypeForPart("word/document.xml");
    ASSERT_TRUE(docType.isOk());
    EXPECT_EQ(docType.value(), kDocxDocument);

    // Verify _rels/.rels exists and parses
    auto relsBack = reader.readEntry("_rels/.rels");
    ASSERT_TRUE(relsBack.isOk());
    auto relsParsed = RelationshipMap::parse(relsBack.value());
    ASSERT_TRUE(relsParsed.isOk());
    auto rId1 = relsParsed.value().getById("rId1");
    ASSERT_TRUE(rId1.isOk());
    EXPECT_EQ(rId1.value()->target, "word/document.xml");

    // Verify word/document.xml exists
    auto docBack = reader.readEntry("word/document.xml");
    ASSERT_TRUE(docBack.isOk());
    std::string docStr(reinterpret_cast<const char*>(docBack.value().data()),
                       docBack.value().size());
    EXPECT_TRUE(docStr.find("<w:document") != std::string::npos);
}

TEST(IntegrationTest, WriteOrderContentTypesFirst) {
    TempFile tmp(tempPath("test_write_order.docx"));

    LibzipZipArchive writer;
    ASSERT_TRUE(writer.create(tmp.path).isOk());

    auto ctXml = ContentTypeRegistry::generate({"word/document.xml"});
    ASSERT_TRUE(writer.writeEntry("[Content_Types].xml", ctXml).isOk());
    ASSERT_TRUE(
        writer
            .writeEntry("_rels/.rels", RelationshipMap::generate({
                                           {"rId1", kOfficeDocRel, "word/document.xml", false},
                                       }))
            .isOk());
    ASSERT_TRUE(writer.writeEntry("word/document.xml", makeMinimalDocumentXml()).isOk());
    ASSERT_TRUE(writer.close().isOk());

    // Verify first entry is [Content_Types].xml
    LibzipZipArchive reader;
    ASSERT_TRUE(reader.open(tmp.path).isOk());
    auto entries = reader.entries();
    ASSERT_TRUE(entries.isOk());
    ASSERT_GE(entries.value().size(), 3u);
    EXPECT_EQ(entries.value()[0].name, "[Content_Types].xml");
}

TEST(IntegrationTest, AllPartsHaveValidMimeTypes) {
    TempFile tmp(tempPath("test_all_mime.docx"));

    LibzipZipArchive writer;
    ASSERT_TRUE(writer.create(tmp.path).isOk());

    auto ctXml = ContentTypeRegistry::generate({"word/document.xml"});
    // std::cout<<std::string(ctXml.begin(),ctXml.end());
    ASSERT_TRUE(writer.writeEntry("[Content_Types].xml", ctXml).isOk());
    ASSERT_TRUE(
        writer
            .writeEntry("_rels/.rels", RelationshipMap::generate({
                                           {"rId1", kOfficeDocRel, "word/document.xml", false},
                                       }))
            .isOk());
    ASSERT_TRUE(writer.writeEntry("word/document.xml", makeMinimalDocumentXml()).isOk());
    ASSERT_TRUE(writer.close().isOk());

    // Read back and verify every part has a valid MIME type
    LibzipZipArchive reader;
    ASSERT_TRUE(reader.open(tmp.path).isOk());
    auto entries = reader.entries();
    ASSERT_TRUE(entries.isOk());

    auto ctBack = reader.readEntry("[Content_Types].xml");
    ASSERT_TRUE(ctBack.isOk());
    auto ctParsed = ContentTypeRegistry::parse(ctBack.value());
    ASSERT_TRUE(ctParsed.isOk());

    for (const auto& entry : entries.value()) {
        if (entry.isDirectory)
            continue;
        // std::cout<<entry.name<<std::endl;
        auto mime = ctParsed.value().getTypeForPart(entry.name);
        EXPECT_TRUE(mime.isOk()) << mime.message();
    }
}

// ============================================================
// Acceptance: generate a docx openable by WPS / MS Word
// ============================================================

// Known relationship types
static const char* kRelCoreProps =
    "http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties";
static const char* kRelExtProps =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties";
static const char* kRelStyles =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles";
static const char* kRelSettings =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/settings";
static const char* kRelFontTable =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/fontTable";
static const char* kRelTheme =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme";

static std::vector<uint8_t> strToBytes(const char* s) {
    std::string str(s);
    return std::vector<uint8_t>(str.begin(), str.end());
}

static std::vector<uint8_t> makeCorePropsXml() {
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<cp:coreProperties"
        " xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\""
        " xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
        " xmlns:dcterms=\"http://purl.org/dc/terms/\""
        " xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\""
        " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"/>";
    return strToBytes(xml);
}

static std::vector<uint8_t> makeAppPropsXml() {
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Properties "
        "xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\"/>";
    return strToBytes(xml);
}

static std::vector<uint8_t> makeStylesXml() {
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii=\"Calibri\" w:hAnsi=\"Calibri\"/>"
        "<w:sz w:val=\"22\"/></w:rPr></w:rPrDefault></w:docDefaults></w:styles>";
    return strToBytes(xml);
}

static std::vector<uint8_t> makeSettingsXml() {
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:settings xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"/>";
    return strToBytes(xml);
}

static std::vector<uint8_t> makeFontTableXml() {
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:fonts xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"/>";
    return strToBytes(xml);
}

static std::vector<uint8_t> makeThemeXml() {
    const char* xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<a:theme xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
        "name=\"Default\">"
        "<a:themeElements>"
        "<a:clrScheme name=\"Default\"><a:dk1><a:sysClr val=\"000000\" lastClr=\"000000\"/></a:dk1>"
        "<a:lt1><a:sysClr val=\"FFFFFF\" lastClr=\"FFFFFF\"/></a:lt1>"
        "<a:dk2><a:srgbClr val=\"44546A\"/></a:dk2>"
        "<a:lt2><a:srgbClr val=\"E7E6E6\"/></a:lt2>"
        "<a:accent1><a:srgbClr val=\"4472C4\"/></a:accent1>"
        "<a:accent2><a:srgbClr val=\"ED7D31\"/></a:accent2>"
        "<a:accent3><a:srgbClr val=\"A5A5A5\"/></a:accent3>"
        "<a:accent4><a:srgbClr val=\"FFC000\"/></a:accent4>"
        "<a:accent5><a:srgbClr val=\"5B9BD5\"/></a:accent5>"
        "<a:accent6><a:srgbClr val=\"70AD47\"/></a:accent6>"
        "<a:hlink><a:srgbClr val=\"0563C1\"/></a:hlink>"
        "<a:folHlink><a:srgbClr val=\"954F72\"/></a:folHlink>"
        "</a:clrScheme>"
        "<a:fontScheme name=\"Default\">"
        "<a:majorFont><a:latin typeface=\"Calibri\"/><a:ea typeface=\"\"/><a:cs "
        "typeface=\"\"/></a:majorFont>"
        "<a:minorFont><a:latin typeface=\"Calibri\"/><a:ea typeface=\"\"/><a:cs "
        "typeface=\"\"/></a:minorFont>"
        "</a:fontScheme>"
        "<a:fmtScheme name=\"Default\"><a:fillStyleLst>"
        "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
        "</a:fillStyleLst><a:lnStyleLst>"
        "<a:ln w=\"12700\"><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill></a:ln>"
        "</a:lnStyleLst></a:fmtScheme>"
        "</a:themeElements></a:theme>";
    return strToBytes(xml);
}

// TEST(IntegrationTest, GenerateValidDocxForWps) {
//     // use isRemain=true to keep the file for manual verification
//     TempFile tmp(tempPath("acceptance_blank.docx"), true);
//     std::cout << "Output: " << tmp.path << std::endl;

//     LibzipZipArchive writer;
//     ASSERT_TRUE(writer.create(tmp.path).isOk());

//     // Step 1: [Content_Types].xml (first entry per OOXML spec)
//     ASSERT_TRUE(writer.writeEntry("[Content_Types].xml",
//         ContentTypeRegistry::generate({
//             "_rels/.rels",
//             "word/document.xml",
//             "word/_rels/document.xml.rels",
//             // "docProps/core.xml",
//             // "docProps/app.xml",
//             // "word/styles.xml",
//             // "word/settings.xml",
//             // "word/fontTable.xml",
//             // "word/theme/theme1.xml",
//         })).isOk());

//     // Step 2: Root relationships
//     ASSERT_TRUE(writer.writeEntry("_rels/.rels",
//         RelationshipMap::generate({
//             {"rId1", kOfficeDocRel, "word/document.xml", false},
//             // {"rId2", kRelCoreProps, "docProps/core.xml", false},
//             // {"rId3", kRelExtProps, "docProps/app.xml", false},
//         })).isOk());

//     // Step 3: Document
//     ASSERT_TRUE(writer.writeEntry("word/document.xml", makeMinimalDocumentXml()).isOk());

//     // // Step 4: Document-level relationships
//     // ASSERT_TRUE(writer.writeEntry("word/_rels/document.xml.rels",
//     //     RelationshipMap::generate({
//     //         {"rId1", kRelStyles,   "styles.xml", false},
//     //         {"rId2", kRelSettings, "settings.xml", false},
//     //         {"rId3", kRelFontTable,"fontTable.xml", false},
//     //         {"rId4", kRelTheme,    "theme/theme1.xml", false},
//     //     })).isOk());

//     // // Step 5-6: docProps
//     // ASSERT_TRUE(writer.writeEntry("docProps/core.xml", makeCorePropsXml()).isOk());
//     // ASSERT_TRUE(writer.writeEntry("docProps/app.xml", makeAppPropsXml()).isOk());

//     // // Step 7-10: Document support parts
//     // ASSERT_TRUE(writer.writeEntry("word/styles.xml", makeStylesXml()).isOk());
//     // ASSERT_TRUE(writer.writeEntry("word/settings.xml", makeSettingsXml()).isOk());
//     // ASSERT_TRUE(writer.writeEntry("word/fontTable.xml", makeFontTableXml()).isOk());
//     // ASSERT_TRUE(writer.writeEntry("word/theme/theme1.xml", makeThemeXml()).isOk());

//     ASSERT_TRUE(writer.close().isOk());

//     // === Verify ===
//     LibzipZipArchive reader;
//     ASSERT_TRUE(reader.open(tmp.path).isOk());
//     auto entries = reader.entries();
//     ASSERT_TRUE(entries.isOk());
//     // ASSERT_GE(entries.value().size(), 10u);

//     // Read back [Content_Types].xml and verify all parts have valid MIME types
//     auto ctBack = reader.readEntry("[Content_Types].xml");
//     ASSERT_TRUE(ctBack.isOk());
//     auto ctParsed = ContentTypeRegistry::parse(ctBack.value());
//     ASSERT_TRUE(ctParsed.isOk());

//     for (const auto& entry : entries.value()) {
//         if (entry.isDirectory) continue;
//         auto mime = ctParsed.value().getTypeForPart(entry.name);
//         EXPECT_TRUE(mime.isOk())
//             << "Part '" << entry.name << "' has no MIME type";
//     }
// }
