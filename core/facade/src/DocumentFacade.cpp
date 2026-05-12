#include "oso/facade/DocumentFacade.h"

#include "oso/base/ErrorCode.h"
#include "oso/dom/word/WordDocument.h"
#include "oso/io/IStream.h"
#include "oso/ooxml/OoxmlSaxHandler.h"
#include "oso/ooxml/common/ContentTypeRegistry.h"
#include "oso/ooxml/common/IZipArchive.h"
#include "oso/ooxml/common/LibzipZipArchive.h"
#include "oso/ooxml/common/OoxmlNamespaces.h"
#include "oso/ooxml/common/RelationshipMap.h"
#include "oso/ooxml/read/Libxml2Reader.h"
#include "oso/ooxml/write/Libxml2Writer.h"

#include <cstring>

namespace oso {

// ============================================================
// 文档类型检测
// ============================================================

Result<std::string> DocumentFacade::detectDocumentType(IZipArchive& archive) {
    // 读取 [Content_Types].xml
    auto ctResult = archive.readEntry("[Content_Types].xml");
    if (ctResult.isErr()) {
        return Result<std::string>::err(ctResult.error(), ctResult.message());
    }

    auto ctParseResult = ContentTypeRegistry::parse(ctResult.value());
    if (ctParseResult.isErr()) {
        return Result<std::string>::err(ctParseResult.error(), ctParseResult.message());
    }

    auto& ct = ctParseResult.value();

    // 尝试通过已知部件查询 MIME 类型
    auto wdResult = ct.getTypeForPart("word/document.xml");
    if (wdResult.isOk() &&
        wdResult.value() == ContentTypeRegistry::lookupOverride("word/document.xml")) {
        return Result<std::string>::ok("word");
    }

    auto wbResult = ct.getTypeForPart("xl/workbook.xml");
    if (wbResult.isOk() &&
        wbResult.value() == ContentTypeRegistry::lookupOverride("xl/workbook.xml")) {
        return Result<std::string>::ok("sheet");
    }

    auto presResult = ct.getTypeForPart("ppt/presentation.xml");
    if (presResult.isOk() &&
        presResult.value() == ContentTypeRegistry::lookupOverride("ppt/presentation.xml")) {
        return Result<std::string>::ok("slide");
    }

    return Result<std::string>::err(ErrorCode::OOXML_InvalidSchema,
                                    "Unable to detect document type");
}

// ============================================================
// 打开文档
// ============================================================

Result<std::unique_ptr<DomNode>> DocumentFacade::openDocument(const std::string& path) {
    // 1. 打开 Zip 档案
    LibzipZipArchive archive;
    auto openResult = archive.open(path);
    if (openResult.isErr()) {
        return Result<std::unique_ptr<DomNode>>::err(openResult.error(), openResult.message());
    }

    // 2. 检测文档类型
    auto typeResult = detectDocumentType(archive);
    if (typeResult.isErr()) {
        return Result<std::unique_ptr<DomNode>>::err(typeResult.error(), typeResult.message());
    }

    // 3. 根据类型路由到对应加载器
    const auto& docType = typeResult.value();
    if (docType == "word") {
        return loadWordDocument(archive);
    }

    return Result<std::unique_ptr<DomNode>>::err(ErrorCode::OOXML_InvalidSchema,
                                                 "Unsupported document type: " + docType);
}

// ============================================================
// 加载 Word 文档
// ============================================================

Result<std::unique_ptr<DomNode>> DocumentFacade::loadWordDocument(IZipArchive& archive) {
    // 1. 读取主文档部件路径（通过 _rels/.rels 找到 officeDocument 关系）
    auto relsResult = archive.readEntry("_rels/.rels");
    if (relsResult.isErr()) {
        return Result<std::unique_ptr<DomNode>>::err(relsResult.error(), relsResult.message());
    }

    auto relsParseResult = RelationshipMap::parse(relsResult.value());
    if (relsParseResult.isErr()) {
        return Result<std::unique_ptr<DomNode>>::err(relsParseResult.error(),
                                                     relsParseResult.message());
    }

    auto& rels = relsParseResult.value();
    auto docRelations = rels.getAllByType(OoxmlNamespaces::kRelOfficeDocument);
    if (docRelations.empty()) {
        return Result<std::unique_ptr<DomNode>>::err(ErrorCode::OOXML_MissingPart,
                                                     "No officeDocument relationship found");
    }

    std::string docPartPath = docRelations[0]->target;

    // 2. 读取主文档 XML
    auto docXmlResult = archive.readEntry(docPartPath);
    if (docXmlResult.isErr()) {
        return Result<std::unique_ptr<DomNode>>::err(docXmlResult.error(), docXmlResult.message());
    }

    // 3. SAX 解析 → DOM
    OoxmlSaxHandler handler("word");
    Libxml2Reader reader;

    auto parseResult = reader.parse(docXmlResult.value(), handler.onStartElement(),
                                    handler.onEndElement(), handler.onCharacters());

    if (parseResult.isErr()) {
        return Result<std::unique_ptr<DomNode>>::err(parseResult.error(), parseResult.message());
    }

    return Result<std::unique_ptr<DomNode>>::ok(handler.releaseDocument());
}

// ============================================================
// 保存文档
// ============================================================

Result<void> DocumentFacade::saveDocument(const DomNode& doc, const std::string& path) {
    auto* docNode = dynamic_cast<const DomDocument*>(&doc);
    if (!docNode) {
        // 包装一层：如果不是 DomDocument，也得保存
    }

    const auto& docType = dynamic_cast<const DomDocument*>(&doc)
                              ? dynamic_cast<const DomDocument*>(&doc)->documentType()
                              : "word";

    if (docType == "word") {
        return saveWordDocument(doc, path);
    }

    return Result<void>::err(ErrorCode::OOXML_InvalidSchema,
                             "Unsupported document type: " + docType);
}

// ============================================================
// 保存 Word 文档
// ============================================================

Result<void> DocumentFacade::saveWordDocument(const DomNode& doc, const std::string& path) {
    // 1. 将 DOM 序列化为 XML
    MemoryStream docStream;
    Libxml2Writer writer;
    writer.setOutput(docStream);

    // 跳过 DomDocument 的 XML 声明（WordDocument::serialize 会自己写）
    auto* wdDoc = dynamic_cast<const WordDocument*>(&doc);
    if (wdDoc) {
        wdDoc->serialize(writer);
    } else {
        doc.serialize(writer);
    }

    // 2. 创建 Zip 文件，按 OOXML 规范顺序写入
    LibzipZipArchive archive;
    auto createResult = archive.create(path);
    if (createResult.isErr()) {
        return createResult;
    }

    // 2a. 首先写 [Content_Types].xml
    auto ctXml = ContentTypeRegistry::generate({"word/document.xml"});
    auto ctWriteResult = archive.writeEntry("[Content_Types].xml", ctXml);
    if (ctWriteResult.isErr())
        return ctWriteResult;

    // std::vector<>

    // 2b. 写 _rels/.rels
    auto relsXml = RelationshipMap::generate({
        oso::RelationshipMap::Relationship("rId1", std::string(OoxmlNamespaces::kRelOfficeDocument),
                                           "word/document.xml", false),
    });
    auto relsWriteResult = archive.writeEntry("_rels/.rels", relsXml);
    if (relsWriteResult.isErr())
        return relsWriteResult;

    // 2c. 最后写 word/document.xml
    auto docWriteResult = archive.writeEntry("word/document.xml", docStream.data());
    if (docWriteResult.isErr())
        return docWriteResult;

    return archive.close();
}

}  // namespace oso
