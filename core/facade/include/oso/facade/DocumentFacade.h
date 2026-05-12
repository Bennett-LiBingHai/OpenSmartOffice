#pragma once

#include "oso/base/Result.h"
#include "oso/dom/common/DomNode.h"
#include <memory>
#include <string>

namespace oso {

// ============================================================
// DocumentFacade — 文档操作统一入口
//
// 提供文档打开/保存的统一 API：
// - 自动检测文档类型（docx/xlsx/pptx），路由到对应加载器
// - 将 DOM 树序列化写回 OOXML Zip 包
//
// M1 阶段仅支持 Word (docx) 格式的打开和 round-trip。
// ============================================================
class DocumentFacade {
public:
    DocumentFacade() = default;

    /// 打开 OOXML 文档
    /// @param path 文件路径（.docx / .xlsx / .pptx）
    /// @return 成功返回文档根节点（WordDocument / Workbook / Presentation）
    Result<std::unique_ptr<DomNode>> openDocument(const std::string& path);

    /// 保存 DOM 文档到 OOXML 文件
    /// @param doc 文档根节点
    /// @param path 输出文件路径
    /// @return 成功返回 OK
    Result<void> saveDocument(const DomNode& doc, const std::string& path);

private:
    /// 通过 [Content_Types].xml 检测文档类型
    /// @return "word" / "sheet" / "slide"
    static Result<std::string> detectDocumentType(class IZipArchive& archive);

    /// 加载 Word 文档的各个部件
    Result<std::unique_ptr<DomNode>> loadWordDocument(class IZipArchive& archive);

    /// 保存 Word 文档
    Result<void> saveWordDocument(const DomNode& doc, const std::string& path);
};

} // namespace oso
