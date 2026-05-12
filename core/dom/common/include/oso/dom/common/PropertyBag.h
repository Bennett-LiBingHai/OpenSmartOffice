#pragma once
#include <unordered_map>
#include <string>

namespace oso{

// ============================================================
// PropertyBag — 属性容器（辅助类）
//
// 用于在 DOM 对象中暂存未识别的 OOXML 属性，
// 在序列化时原样写出，保障无损读写。
// 与 DomNode::m_attributes 配合使用。
// ============================================================
class PropertyBag {
public:
    void set(const std::string& key, const std::string& value) {
        m_props[key] = value;
    }

    const std::string& get(const std::string& key, const std::string& defaultValue = "") const {
        auto it = m_props.find(key);
        return it != m_props.end() ? it->second : defaultValue;
    }

    bool has(const std::string& key) const {
        return m_props.find(key) != m_props.end();
    }

    const std::unordered_map<std::string, std::string>& all() const {
        return m_props;
    }

    bool empty() const { return m_props.empty(); }

private:
    std::unordered_map<std::string, std::string> m_props;
    static const std::string kEmpty;
};

}//namespace oso