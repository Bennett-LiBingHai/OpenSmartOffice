#include <gtest/gtest.h>
#include "oso/facade/DocumentFacade.h"
#include "oso/dom/word/WordDocument.h"
#include "oso/dom/common/DomNode.h"
#include "oso/ooxml/common/IZipArchive.h"
#include "oso/ooxml/common/LibzipZipArchive.h"
#include "oso/ooxml/common/ContentTypeRegistry.h"
#include "oso/ooxml/common/RelationshipMap.h"
#include "oso/base/ErrorCode.h"
#include <cstdio>
#include <filesystem>
#include <cstring>

using namespace oso;
namespace fs = std::filesystem;

namespace {

struct TempFile {
    std::string path;
    bool isRemain;
    explicit TempFile(std::string p, bool ir = false) : path(std::move(p)), isRemain(ir) {}
    ~TempFile() { if (!isRemain) std::remove(path.c_str()); }
};

std::string tempPath(const std::string& name) {
    return (fs::temp_directory_path() / name).string();
}

} // anonymous namespace

// ================================================================
// DocumentFacade::openDocument — 错误路径
// ================================================================

TEST(DocumentFacade, OpenNonExistentFile) {
    DocumentFacade facade;
    auto result = facade.openDocument("/nonexistent/path/file.docx");
    EXPECT_TRUE(result.isErr());
    EXPECT_EQ(result.error(), ErrorCode::IO_FileNotFound);
}

TEST(DocumentFacade, OpenFileThatIsNotAZip) {
    // 创建一个非 Zip 文件
    TempFile tmp(tempPath("test_not_a_zip.docx"));
    {
        auto* fd = std::fopen(tmp.path.c_str(), "wb");
        ASSERT_NE(fd, nullptr);
        const char* content = "This is not a zip file!";
        std::fwrite(content, 1, std::strlen(content), fd);
        std::fclose(fd);
    }

    DocumentFacade facade;
    auto result = facade.openDocument(tmp.path);
    EXPECT_TRUE(result.isErr());
}

TEST(DocumentFacade, OpenEmptyFile) {
    TempFile tmp(tempPath("test_empty.docx"));
    {
        auto* fd = std::fopen(tmp.path.c_str(), "wb");
        ASSERT_NE(fd, nullptr);
        std::fclose(fd);
    }

    DocumentFacade facade;
    auto result = facade.openDocument(tmp.path);
    EXPECT_TRUE(result.isErr());
}

// ================================================================
// DocumentFacade::saveDocument — 错误路径
// ================================================================

TEST(DocumentFacade, SaveToReadOnlyPath) {
    DocumentFacade facade;

    auto openResult = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");
    ASSERT_TRUE(openResult.isOk()) << openResult.message();
    auto doc = openResult.takeValue();

    // 尝试保存到不可写的路径
    auto result = facade.saveDocument(*doc, "/root/cannot_write_here.docx");
    EXPECT_TRUE(result.isErr());
}

TEST(DocumentFacade, SaveNullDocument) {
    // saveDocument 接收一个空的 WordDocument 应该也能工作
    WordDocument emptyDoc;
    TempFile tmp(tempPath("test_empty_save.docx"));

    DocumentFacade facade;
    auto result = facade.saveDocument(emptyDoc, tmp.path);
    // 空文档保存应成功（生成最小结构）
    ASSERT_TRUE(result.isOk()) << result.message();

    // 验证生成的文件可被重新打开
    auto reopenResult = facade.openDocument(tmp.path);
    ASSERT_TRUE(reopenResult.isOk()) << reopenResult.message();
}

// ================================================================
// DocumentFacade — 文档检测
// ================================================================

TEST(DocumentFacade, DetectsWordDocument) {
    DocumentFacade facade;
    auto result = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");
    ASSERT_TRUE(result.isOk()) << result.message();

    auto doc = result.takeValue();
    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    ASSERT_NE(wordDoc, nullptr);
    EXPECT_EQ(wordDoc->documentType(), "word");
}

// ================================================================
// DocumentFacade — Round-trip 完整工作流
// ================================================================

TEST(DocumentFacade, BasicRoundTripPreservesStructure) {
    DocumentFacade facade;

    // 1. 打开原始文档
    auto openResult = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");
    ASSERT_TRUE(openResult.isOk()) << openResult.message();
    auto doc = openResult.takeValue();

    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    ASSERT_NE(wordDoc, nullptr);

    // 2. 保存
    TempFile tmp(tempPath("test_facade_structure_roundtrip.docx"));
    auto saveResult = facade.saveDocument(*doc, tmp.path);
    ASSERT_TRUE(saveResult.isOk()) << saveResult.message();

    // 3. 重新打开
    auto reopenResult = facade.openDocument(tmp.path);
    ASSERT_TRUE(reopenResult.isOk()) << reopenResult.message();
    auto reopened = reopenResult.takeValue();

    // 4. 验证结构一致
    auto* rwDoc = dynamic_cast<WordDocument*>(reopened.get());
    ASSERT_NE(rwDoc, nullptr);
    ASSERT_NE(rwDoc->body(), nullptr);
    EXPECT_EQ(rwDoc->localName(), "document");
    EXPECT_EQ(rwDoc->documentType(), "word");
}

TEST(DocumentFacade, RoundTripPreservesZipStructure) {
    DocumentFacade facade;

    auto openResult = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");
    ASSERT_TRUE(openResult.isOk());
    auto doc = openResult.takeValue();

    TempFile tmp(tempPath("test_facade_zip_structure.docx"));
    ASSERT_TRUE(facade.saveDocument(*doc, tmp.path).isOk());

    // 验证 Zip 结构完整
    LibzipZipArchive archive;
    ASSERT_TRUE(archive.open(tmp.path).isOk());

    auto entries = archive.entries();
    ASSERT_TRUE(entries.isOk());
    auto& list = entries.value();

    // 必须有这三个部件
    bool hasContentTypes = false, hasRels = false, hasDocument = false;
    for (const auto& e : list) {
        if (e.name == "[Content_Types].xml") hasContentTypes = true;
        if (e.name == "_rels/.rels") hasRels = true;
        if (e.name == "word/document.xml") hasDocument = true;
    }

    EXPECT_TRUE(hasContentTypes);
    EXPECT_TRUE(hasRels);
    EXPECT_TRUE(hasDocument);

    // 验证 [Content_Types].xml 内容合法
    auto ctData = archive.readEntry("[Content_Types].xml");
    ASSERT_TRUE(ctData.isOk());
    auto ct = ContentTypeRegistry::parse(ctData.value());
    ASSERT_TRUE(ct.isOk());

    // 验证 _rels/.rels 内容合法
    auto relsData = archive.readEntry("_rels/.rels");
    ASSERT_TRUE(relsData.isOk());
    auto rels = RelationshipMap::parse(relsData.value());
    ASSERT_TRUE(rels.isOk());

    // 验证 document.xml 的 MIME 类型正确
    auto mimeType = ct.value().getTypeForPart("word/document.xml");
    ASSERT_TRUE(mimeType.isOk());
    EXPECT_EQ(mimeType.value(),
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml");
}

TEST(DocumentFacade, DoubleOpenDoesNotCrash) {
    DocumentFacade facade;

    auto r1 = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");
    ASSERT_TRUE(r1.isOk());
    auto doc1 = r1.takeValue();

    auto r2 = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");
    ASSERT_TRUE(r2.isOk());
    auto doc2 = r2.takeValue();

    // 两个文档应该是独立的实例
    ASSERT_NE(doc1, nullptr);
    ASSERT_NE(doc2, nullptr);
    EXPECT_EQ(doc1->localName(), doc2->localName());
}

TEST(DocumentFacade, OpenModifiedByAddingParagraphThenSaveAndReopen) {
    DocumentFacade facade;

    // 1. 打开
    auto openResult = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");
    ASSERT_TRUE(openResult.isOk());
    auto doc = openResult.takeValue();

    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    ASSERT_NE(wordDoc, nullptr);
    ASSERT_NE(wordDoc->body(), nullptr);

    size_t originalChildCount = wordDoc->body()->childCount();

    // 2. 修改：添加一个段落
    auto newPara = std::make_unique<Paragraph>();
    auto newRun = std::make_unique<oso::Run>();
    auto newText = std::make_unique<Text>();
    newText->setContent(std::make_unique<DomText>("Modified content"));
    newRun->addText(std::move(newText));
    newPara->addRun(std::move(newRun));
    wordDoc->body()->addParagraph(std::move(newPara));

    EXPECT_GT(wordDoc->body()->childCount(), originalChildCount);

    // 3. 保存
    TempFile tmp(tempPath("test_facade_modified.docx"));
    ASSERT_TRUE(facade.saveDocument(*doc, tmp.path).isOk());

    // 4. 重新打开
    auto reopenResult = facade.openDocument(tmp.path);
    ASSERT_TRUE(reopenResult.isOk());
    auto reopened = reopenResult.takeValue();

    auto* rwDoc = dynamic_cast<WordDocument*>(reopened.get());
    ASSERT_NE(rwDoc, nullptr);
    ASSERT_NE(rwDoc->body(), nullptr);

    // 5. 验证修改已保存
    auto* lastPara = rwDoc->body()->lastParagraph();
    ASSERT_NE(lastPara, nullptr);
    auto* lastRun = lastPara->lastRun();
    ASSERT_NE(lastRun, nullptr);
    EXPECT_EQ(lastRun->textContent(), "Modified content");
}

// ================================================================
// DocumentFacade — 连续 round-trip（验证稳定性）
// ================================================================

TEST(DocumentFacade, RepeatedRoundTripIsStable) {
    DocumentFacade facade;

    auto openResult = facade.openDocument(std::string(TEST_FIXTURE_DIR) + "/minimal.docx");
    ASSERT_TRUE(openResult.isOk());
    auto doc = openResult.takeValue();

    // 连续 round-trip 3 次
    for (int i = 0; i < 3; ++i) {
        TempFile tmp(tempPath("test_stability_roundtrip_" + std::to_string(i) + ".docx"));
        ASSERT_TRUE(facade.saveDocument(*doc, tmp.path).isOk());

        auto reopenResult = facade.openDocument(tmp.path);
        ASSERT_TRUE(reopenResult.isOk());
        doc = reopenResult.takeValue();
    }

    // 最终结果应仍有正确的结构
    auto* wordDoc = dynamic_cast<WordDocument*>(doc.get());
    ASSERT_NE(wordDoc, nullptr);
    ASSERT_NE(wordDoc->body(), nullptr);
    EXPECT_GE(wordDoc->body()->childCount(), 1u);
}
