#pragma once

#include "oso/base/Result.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oso {

// [Content_Types].xml
// <Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
// <Default Extension="png" ContentType="image/png"/>
// <Default Extension="rels"
// ContentType="application/vnd.openxmlformats-package.relationships+xml"/> <Default Extension="xml"
// ContentType="application/xml"/> <Override PartName="/docProps/app.xml"
// ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/> <Override
// PartName="/docProps/core.xml"
// ContentType="application/vnd.openxmlformats-package.core-properties+xml"/> <Override
// PartName="/docProps/custom.xml"
// ContentType="application/vnd.openxmlformats-officedocument.custom-properties+xml"/> <Override
// PartName="/word/document.xml"
// ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
// </Types>
class ContentTypeRegistry {
public:
    //</Types>中的部件
    struct Entry {
        std::string partName;  // 部件路径（Override）或扩展名（Default），不含前导 "/"
        std::string mimeType;  // MIME 类型
        bool isOverride;  // true = Override, false = Default
    };

    // 解析[Content_Types].xml
    static Result<ContentTypeRegistry> parse(const std::vector<uint8_t>& xmlData);

    /**
     * @brief 根据部件路径列表生成 [Content_Types].xml
     *
     * ContentType 完全自动解析：文件名匹配查 Override，扩展名匹配查 Default。
     * rels 和 xml 两个 Default 始终写入。
     * @param partNames 部件路径列表，路径不含前导 "/"，如 {"word/document.xml", "_rels/.rels"}
     * @return 完整的 [Content_Types].xml 字节数据
     */
    static std::vector<uint8_t> generate(const std::vector<std::string>& partNames);

    // 获取Override的ContentType，如果失败则提取扩展名尝试用getTypeForExtension解析
    Result<std::string> getTypeForPart(std::string_view partPath) const;
    // 获取Default的ContentType
    Result<std::string> getTypeForExtension(std::string_view ext) const;
    // 获取所有部件
    const std::vector<Entry>& allTypes() const { return m_entries; }

    ContentTypeRegistry() = default;

    // 用 partName 查 Override 表，支持前缀匹配（如 "xl/worksheets/sheet1.xml" 匹配
    // "xl/worksheets/sheet"） 找不到返回空字符串
    static std::string_view lookupOverride(const std::string& partName);

    // 用扩展名查 Default 表
    // 找不到返回空字符串
    static std::string_view lookupDefault(const std::string& ext);

private:
    // Extension -> Mime映射常量
    static const std::unordered_map<std::string_view, std::string_view> kExtensionMime;

    // 文件名（或前缀）→ Override ContentType
    // 路径不含前导 "/"，带前缀匹配用于编号文件（如 sheet1.xml, slide2.xml）
    static const std::unordered_map<std::string_view, std::string_view> kPartMime;

    std::unordered_map<std::string, std::string> m_overrides;  // partPath → MIME
    std::unordered_map<std::string, std::string> m_defaults;  // extension → MIME
    std::vector<Entry> m_entries;
};

}  // namespace oso
