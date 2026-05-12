#pragma once

#include "oso/base/Result.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oso {

// 解析.rels文件
// <Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
// <Relationship Id="rId4"
// Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument"
// Target="word/document.xml"/> <Relationship Id="rId2"
// Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties"
// Target="docProps/core.xml"/> <Relationship Id="rId1"
// Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties"
// Target="docProps/app.xml"/> <Relationship Id="rId3"
// Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/custom-properties"
// Target="docProps/custom.xml"/>
// </Relationships>
class RelationshipMap {
   public:
    // Relationship标签实体
    struct Relationship {
        std::string id;  // rId1, rId2, ...
        std::string type;  // 关系类型 URI
        std::string target;  // 目标部件路径或外部 URL
        bool isExternal;  // TargetMode="External"
    };

    // 解析.rels文件
    static Result<RelationshipMap> parse(const std::vector<uint8_t>& xmlData);

    /**
     * @brief 根据关系列表生成 .rels XML
     *
     * @param relationships 关系列表
     * @return 完整的 .rels XML 字节数据
     */
    static std::vector<uint8_t> generate(const std::vector<Relationship>& relationships);

    // 通过id获取对应Relationship实体
    Result<const Relationship*> getById(std::string_view id) const;

    // 通过Target获取对应Relationship实体
    Result<const Relationship*> getByTarget(std::string_view target) const;

    // 获取对应类型的所有Relationship
    std::vector<const Relationship*> getAllByType(std::string_view type) const;

    // 获取所有Relationship标签实体
    const std::vector<Relationship>& getAll() const {
        return m_relationships;
    }

    RelationshipMap() = default;

   private:
    std::vector<Relationship> m_relationships;  // 所有Relationship标签实体
    std::unordered_map<std::string, size_t> m_byId;  // 通过id → 映射m_relationships中的下标
};

}  // namespace oso
