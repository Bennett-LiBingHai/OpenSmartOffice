#pragma once

// ============================================================
// OOXML 命名空间常量 — ECMA-376 / ISO/IEC 29500
// ============================================================
//
// 每个命名空间标注其来源章节（5th Edition, ECMA-376）。
// Part 1 = Fundamentals And Markup Language Reference
// Part 2 = Open Packaging Conventions
// Part 3 = Markup Compatibility and Extensibility
//
// 注意: ISO 29500 使用 http://purl.oclc.org/ooxml/ 作为基础 URI，
// ECMA-376 使用 http://schemas.openxmlformats.org/，两者等价。

#include <string>
#include <string_view>
#include <unordered_map>

namespace oso::ooxml_namespaces {

// ================================================================
// 主要标记语言命名空间 — Part 1
// ================================================================

/// WordprocessingML — Part 1, Clause 11
constexpr std::string_view kWordprocessingML =
    "http://schemas.openxmlformats.org/wordprocessingml/2006/main";

/// SpreadsheetML — Part 1, Clause 12
constexpr std::string_view kSpreadsheetML =
    "http://schemas.openxmlformats.org/spreadsheetml/2006/main";

/// PresentationML — Part 1, Clause 13
constexpr std::string_view kPresentationML =
    "http://schemas.openxmlformats.org/presentationml/2006/main";

/// DrawingML — Part 1, Clause 14 (also Clause 20.1, Annex A.4)
constexpr std::string_view kDrawingML = "http://schemas.openxmlformats.org/drawingml/2006/main";

/// DrawingML Picture — Part 1, Clause 20.2
constexpr std::string_view kDrawingMLPicture =
    "http://schemas.openxmlformats.org/drawingml/2006/picture";

/// DrawingML WordprocessingML Drawing — Part 1, Clause 20.4
constexpr std::string_view kDrawingMLWordprocessing =
    "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing";

/// DrawingML SpreadsheetML Drawing — Part 1, Clause 20.5 / 12.3.8
constexpr std::string_view kDrawingMLSpreadsheet =
    "http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing";

// ================================================================
// 包级别命名空间 — Part 2, Annex E (Table E.1)
// ================================================================

/// Content Types 流 — Part 2, Annex E, Table E.1
constexpr std::string_view kContentTypes =
    "http://schemas.openxmlformats.org/package/2006/content-types";

/// 关系部件 — Part 2, Annex E, Table E.1; Clause 7, Clause 9
constexpr std::string_view kRelationships =
    "http://schemas.openxmlformats.org/package/2006/relationships";

/// 核心属性 — Part 2, Annex E, Table E.1; Clause 8
constexpr std::string_view kCoreProperties =
    "http://schemas.openxmlformats.org/package/2006/metadata/core-properties";

// ================================================================
// 办公文档级命名空间 — Part 1
// ================================================================

/// 扩展属性 — Part 1, Clause 22.2 / 15.2.12.3
constexpr std::string_view kExtendedProperties =
    "http://schemas.openxmlformats.org/officeDocument/2006/extended-properties";

/// 自定义属性 — Part 1, Clause 22.3 / 15.2.12.2
constexpr std::string_view kCustomProperties =
    "http://schemas.openxmlformats.org/officeDocument/2006/custom-properties";

/// 数学标记语言 (OMML) — Part 1, Clause 22.1
constexpr std::string_view kMathML = "http://schemas.openxmlformats.org/officeDocument/2006/math";

/// 自定义 XML 数据 — Part 1, Clause 15.2.4–15.2.6 / Clause 22.5
constexpr std::string_view kCustomXml =
    "http://schemas.openxmlformats.org/officeDocument/2006/customXml";

// ================================================================
// 关系类型 — Part 1, Clause 9.2; Clause 22.8（各 Part 子章节见末尾标注）
// ================================================================

/// 主文档关系 — Part 1: 11.3.10 (Word), 12.3.23 (Excel), 13.3.6 (PPT)
constexpr std::string_view kRelOfficeDocument =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument";

/// 样式表关系 — Part 1: 11.3.12 (Word), 12.3.20 (Excel)
constexpr std::string_view kRelStyles =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles";

/// 文档设置关系 — Part 1: 11.3.3
constexpr std::string_view kRelSettings =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/settings";

/// 字体表关系 — Part 1: 11.3.5
constexpr std::string_view kRelFontTable =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/fontTable";

/// 主题关系 — Part 1: 14.2.7
constexpr std::string_view kRelTheme =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme";

/// 编号定义关系 — Part 1: 11.3.11
constexpr std::string_view kRelNumbering =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering";

/// 页眉关系 — Part 1: 11.3.9
constexpr std::string_view kRelHeader =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/header";

/// 页脚关系 — Part 1: 11.3.6
constexpr std::string_view kRelFooter =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer";

/// 批注关系 — Part 1: 11.3.2 (Word), 12.3.3 (Excel), 13.3.2 (PPT)
constexpr std::string_view kRelComments =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/comments";

/// 图片关系 — Part 1: 15.2.14
constexpr std::string_view kRelImage =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image";

/// 超链接关系 — Part 1: 15.3
constexpr std::string_view kRelHyperlink =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink";

/// 图表关系 — Part 1: 14.2.1
constexpr std::string_view kRelChart =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart";

/// 工作表关系 — Part 1: 12.3.24
constexpr std::string_view kRelWorksheet =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet";

/// 共享字符串表关系 — Part 1: 12.3.15
constexpr std::string_view kRelSharedStrings =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings";

/// 幻灯片关系 — Part 1: 13.3.8
constexpr std::string_view kRelSlide =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide";

/// 幻灯片母版关系 — Part 1: 13.3.10
constexpr std::string_view kRelSlideMaster =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster";

/// 幻灯片版式关系 — Part 1: 13.3.9
constexpr std::string_view kRelSlideLayout =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout";

/// 备注幻灯片关系 — Part 1: 13.3.5
constexpr std::string_view kRelNotesSlide =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/notesSlide";

// ================================================================
// 外部标准命名空间
// ================================================================

/// XML Namespace — W3C XML Namespaces 1.0 规范；被 Part 3 Clause 5 引用
constexpr std::string_view kXml = "http://www.w3.org/XML/1998/namespace";

/// Dublin Core Elements — Part 2, Clause 8 (Core Properties)
constexpr std::string_view kDublinCore = "http://purl.org/dc/elements/1.1/";

/// Dublin Core Terms — Part 2, Clause 8
constexpr std::string_view kDublinCoreTerms = "http://purl.org/dc/terms/";

/// Dublin Core Type — Part 1, Annex D
constexpr std::string_view kDublinCoreType = "http://purl.org/dc/dcmitype/";

/// Markup Compatibility — Part 3, Clause 6
constexpr std::string_view kMarkupCompatibility =
    "http://schemas.openxmlformats.org/markup-compatibility/2006";

// ================================================================
// 前缀约定
// ================================================================
//
// OOXML 中前缀几乎不变（ECMA-376 Part 1, Annex D 定义完整映射表）。
// 以下仅供参考，实际解析应始终以 SAX2 namespaceUri 为准。

inline const std::unordered_map<std::string_view, std::string_view>& prefixMap() {
    static const std::unordered_map<std::string_view, std::string_view> kMap = {
        {"w", kWordprocessingML},
        {"r", kRelationships},
        {"x", kSpreadsheetML},
        {"p", kPresentationML},
        {"a", kDrawingML},
        {"wp", kDrawingMLWordprocessing},
        {"pic", kDrawingMLPicture},
        {"dsp", kDrawingMLSpreadsheet},
        {"m", kMathML},
        {"ct", kContentTypes},
        {"cp", kCoreProperties},
        {"dc", kDublinCore},
        {"dcterms", kDublinCoreTerms},
        {"dcmitype", kDublinCoreType},
        {"mc", kMarkupCompatibility},
    };
    return kMap;
}

}  // namespace oso::ooxml_namespaces
