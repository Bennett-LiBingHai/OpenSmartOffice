#include "oso/ooxml/common/ContentTypeRegistry.h"

#include "oso/base/ErrorCode.h"
#include "oso/ooxml/common/OoxmlNamespaces.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstring>
#include <sstream>
#include <unordered_set>

namespace oso {

const std::unordered_map<std::string_view, std::string_view> ContentTypeRegistry::kExtensionMime = {
    {"rels", "application/vnd.openxmlformats-package.relationships+xml"},
    {"xml", "application/xml"},
    {"png", "image/png"},
    {"jpeg", "image/jpeg"},
    {"jpg", "image/jpeg"},
    {"gif", "image/gif"},
    {"svg", "image/svg+xml"},
    {"bmp", "image/bmp"},
    {"tiff", "image/tiff"},
    {"tif", "image/tiff"},
    {"wmf", "image/x-wmf"},
    {"emf", "image/x-emf"},
};

const std::unordered_map<std::string_view, std::string_view> ContentTypeRegistry::kPartMime = {
    // WordprocessingML
    {"word/document.xml",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"},
    {"word/comments",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.comments+xml"},
    {"word/settings.xml",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.settings+xml"},
    {"word/endnotes",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.endnotes+xml"},
    {"word/fontTable.xml",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.fontTable+xml"},
    {"word/footer", "application/vnd.openxmlformats-officedocument.wordprocessingml.footer+xml"},
    {"word/footnotes",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.footnotes+xml"},
    {"word/glossary/document.xml",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.document.glossary+xml"},
    {"word/header", "application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml"},
    {"word/numbering.xml",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml"},
    {"word/styles.xml",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"},
    {"word/webSettings.xml",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.webSettings+xml"},

    // SpreadsheetML
    {"xl/workbook.xml",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"},
    {"xl/worksheets/sheet",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"},
    {"xl/styles.xml", "application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"},
    {"xl/sharedStrings.xml",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"},
    {"xl/calcChain.xml",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.calcChain+xml"},
    {"xl/chartsheets/sheet",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.chartsheet+xml"},
    {"xl/comments", "application/vnd.openxmlformats-officedocument.spreadsheetml.comments+xml"},
    {"xl/connections.xml",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.connections+xml"},
    {"xl/pivotTables/pivotTable",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.pivotTable+xml"},
    {"xl/pivotCache/pivotCacheDefinition",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.pivotCacheDefinition+xml"},
    {"xl/pivotCache/pivotCacheRecords",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.pivotCacheRecords+xml"},
    {"xl/tables/table", "application/vnd.openxmlformats-officedocument.spreadsheetml.table+xml"},
    {"xl/queryTables/queryTable",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.queryTable+xml"},
    {"xl/externalLinks/externalLink",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.externalLink+xml"},
    {"xl/metadata.xml",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.sheetMetadata+xml"},
    {"xl/revisions/revisionHeaders",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.revisionHeaders+xml"},
    {"xl/revisions/revisionLog",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.revisionLog+xml"},
    {"xl/revisions/userNames",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.userNames+xml"},
    {"xl/tables/tableSingleCells",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.tableSingleCells+xml"},
    {"xl/volatileDependencies.xml",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.volatileDependencies+xml"},

    // PresentationML
    {"ppt/presentation.xml",
     "application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"},
    {"ppt/slides/slide", "application/vnd.openxmlformats-officedocument.presentationml.slide+xml"},
    {"ppt/slideLayouts/slideLayout",
     "application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml"},
    {"ppt/slideMasters/slideMaster",
     "application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"},
    {"ppt/notesSlides/notesSlide",
     "application/vnd.openxmlformats-officedocument.presentationml.notesSlide+xml"},
    {"ppt/notesMasters/notesMaster",
     "application/vnd.openxmlformats-officedocument.presentationml.notesMaster+xml"},
    {"ppt/presProps.xml",
     "application/vnd.openxmlformats-officedocument.presentationml.presProps+xml"},
    {"ppt/tableStyles.xml",
     "application/vnd.openxmlformats-officedocument.presentationml.tableStyles+xml"},

    // DrawingML / Shared
    {"charts/chart", "application/vnd.openxmlformats-officedocument.drawingml.chart+xml"},
    {"diagrams/colors",
     "application/vnd.openxmlformats-officedocument.drawingml.diagramColors+xml"},
    {"diagrams/data", "application/vnd.openxmlformats-officedocument.drawingml.diagramData+xml"},
    {"diagrams/layout",
     "application/vnd.openxmlformats-officedocument.drawingml.diagramLayout+xml"},
    {"diagrams/quickStyle",
     "application/vnd.openxmlformats-officedocument.drawingml.diagramStyle+xml"},
    {"word/theme/theme", "application/vnd.openxmlformats-officedocument.theme+xml"},

    // docProps
    {"docProps/core.xml", "application/vnd.openxmlformats-package.core-properties+xml"},
    {"docProps/app.xml", "application/vnd.openxmlformats-officedocument.extended-properties+xml"},
    {"docProps/custom.xml", "application/vnd.openxmlformats-officedocument.custom-properties+xml"},
    {"docProps/customXml", "application/vnd.openxmlformats-officedocument.customXmlProperties+xml"},
};

std::string_view ContentTypeRegistry::lookupOverride(const std::string& partName) {
    // 先精确匹配
    for (const auto& [name, mime] : kPartMime) {
        if (partName == name)
            return mime;
    }
    // 再前缀匹配（用于编号文件：sheet1.xml / sheet2.xml / slide3.xml 等）
    for (const auto& [name, mime] : kPartMime) {
        std::size_t len = name.size();
        if (partName.size() > len && partName.compare(0, len, name) == 0) {
            return mime;
        }
    }
    return "";
}

std::string_view ContentTypeRegistry::lookupDefault(const std::string& ext) {
    for (const auto& [e, mime] : kExtensionMime) {
        if (ext == e)
            return mime;
    }
    return "";
}

std::vector<uint8_t> ContentTypeRegistry::generate(const std::vector<std::string>& partNames) {
    // Override 条目（partPath → MIME），去重
    std::unordered_map<std::string, std::string> overrides;
    // 除 rels/xml 之外额外需要的 Default 扩展名（Extension → MIME）
    std::unordered_map<std::string, std::string> extraDefaults;

    for (const auto& path : partNames) {
        auto dot = path.rfind('.');
        std::string ext = (dot != std::string::npos) ? path.substr(dot + 1) : "";

        // .rels 文件始终走 Default，不生成 Override
        if (ext == "rels")
            continue;

        std::string_view mime = lookupOverride(path);
        if (mime.size()) {
            overrides.emplace(path, mime);
        } else {
            mime = lookupDefault(ext);
            // 不在 Override 表中的，检查是否需要加 Default
            if (ext == "xml") {
                continue;
            } else if (mime.size()) {
                extraDefaults.emplace(ext, mime);
            }
            // 如果扩展名也不在 Default 表中 → 仍作为 Override 写入（用 application/xml 兜底）
            else {
                overrides.emplace(path, "application/xml");
            }
        }
    }

    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        << "<Types xmlns=\"" << OoxmlNamespaces::kContentTypes << "\">\n"
        << "  <Default Extension=\"rels\" "
           "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
        << "  <Default Extension=\"xml\" ContentType=\"application/xml\"/>\n";

    for (const auto& [ext, mime] : extraDefaults) {
        xml << "  <Default Extension=\"" << ext << "\" ContentType=\"" << mime << "\"/>\n";
    }

    for (const auto& [path, mime] : overrides) {
        xml << "  <Override PartName=\"/" << path << "\" ContentType=\"" << mime << "\"/>\n";
    }

    xml << "</Types>\n";

    std::string s = xml.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

Result<ContentTypeRegistry> ContentTypeRegistry::parse(const std::vector<uint8_t>& xmlData) {
    if (xmlData.empty()) {
        return Result<ContentTypeRegistry>::err(ErrorCode::OOXML_XmlParseError,
                                                "Empty [Content_Types].xml data");
    }

    xmlDocPtr doc = xmlReadMemory(reinterpret_cast<const char*>(xmlData.data()),
                                  static_cast<int>(xmlData.size()), nullptr, nullptr,
                                  XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_COMPACT);

    if (!doc) {
        return Result<ContentTypeRegistry>::err(ErrorCode::OOXML_XmlParseError,
                                                "Failed to parse [Content_Types].xml");
    }

    ContentTypeRegistry registry;

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root || std::strcmp(reinterpret_cast<const char*>(root->name), "Types") != 0) {
        xmlFreeDoc(doc);
        return Result<ContentTypeRegistry>::err(ErrorCode::OOXML_InvalidSchema,
                                                "[Content_Types].xml root element is not <Types>");
    }

    for (xmlNodePtr child = root->children; child; child = child->next) {
        if (child->type != XML_ELEMENT_NODE)
            continue;

        const char* nodeName = reinterpret_cast<const char*>(child->name);
        bool isOverride = (std::strcmp(nodeName, "Override") == 0);
        bool isDefault = (std::strcmp(nodeName, "Default") == 0);

        if (!isOverride && !isDefault)
            continue;

        xmlChar* partName = xmlGetProp(
            child, reinterpret_cast<const xmlChar*>(isOverride ? "PartName" : "Extension"));
        xmlChar* contentType = xmlGetProp(child, reinterpret_cast<const xmlChar*>("ContentType"));

        if (!partName || !contentType) {
            if (partName)
                xmlFree(partName);
            if (contentType)
                xmlFree(contentType);
            continue;
        }

        std::string nameStr(reinterpret_cast<const char*>(partName));
        std::string mimeStr(reinterpret_cast<const char*>(contentType));

        // 标准化Override PartName：去除开头的斜杠 "/"
        if (isOverride && !nameStr.empty() && nameStr[0] == '/') {
            nameStr = nameStr.substr(1);
        }

        Entry entry;
        entry.partName = nameStr;
        entry.mimeType = mimeStr;
        entry.isOverride = isOverride;
        registry.m_entries.push_back(std::move(entry));

        if (isOverride) {
            registry.m_overrides[nameStr] = mimeStr;
        } else {
            registry.m_defaults[nameStr] = mimeStr;
        }

        xmlFree(partName);
        xmlFree(contentType);
    }

    xmlFreeDoc(doc);
    return Result<ContentTypeRegistry>::ok(std::move(registry));
}

Result<std::string> ContentTypeRegistry::getTypeForPart(std::string_view partPath) const {
    // Strip leading "/" and try Override exact match first
    std::string normalized(partPath);
    if (!normalized.empty() && normalized[0] == '/') {
        normalized = normalized.substr(1);
    }

    auto it = m_overrides.find(normalized);
    if (it != m_overrides.end()) {
        return Result<std::string>::ok(it->second);
    }

    // 回退至基于扩展名的默认设置
    size_t dotPos = normalized.rfind('.');
    if (dotPos == std::string::npos) {
        return Result<std::string>::err(ErrorCode::OOXML_BadContentType,
                                        "No content type found for part: " + std::string(partPath));
    }

    std::string ext = normalized.substr(dotPos + 1);
    return getTypeForExtension(ext);
}

Result<std::string> ContentTypeRegistry::getTypeForExtension(std::string_view ext) const {
    auto it = m_defaults.find(std::string(ext));
    if (it != m_defaults.end()) {
        return Result<std::string>::ok(it->second);
    }
    return Result<std::string>::err(ErrorCode::OOXML_BadContentType,
                                    "No content type for extension: " + std::string(ext));
}

}  // namespace oso
