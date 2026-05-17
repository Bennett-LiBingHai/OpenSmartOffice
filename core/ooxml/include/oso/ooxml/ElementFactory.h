#pragma once
#include "oso/dom/common/DomNode.h"

#include <functional>
#include <memory>

namespace oso {
// ============================================================
// ElementFactory — OOXML 元素 → DOM 对象工厂
//
// 根据 OOXML 元素的命名空间 URI 和本地名称，
// 创建对应的 DOM 节点。支持注册自定义创建函数。
//
// 未注册的元素类型统一创建为 DomElement（泛型节点），
// 保留全部属性和子元素，保障无损读写。
// ============================================================
class ElementFactory {
public:
    /// 元素创建函数签名：接收 localName 和 namespaceUri，返回新建的 DOM 节点
    using CreatorFn = std::function<std::unique_ptr<DomNode>(const std::string& localName,
                                                             const std::string& namespaceUri)>;

    /// 获取全局单例
    static ElementFactory& instance();

    /// 注册一个元素类型的创建函数
    void registerCreator(const std::string& namespaceUri, const std::string& localName,
                         CreatorFn creator);

    /// 创建 DOM 节点。已注册类型使用注册函数，未注册的创建泛型 DomElement
    std::unique_ptr<DomNode> create(const std::string& namespaceUri,
                                    const std::string& localName) const;

    /// 检查某个元素类型是否已注册
    bool isRegistered(const std::string& namespaceUri, const std::string& localName) const;

private:
    ElementFactory() = default;

    /// 组合 namespaceUri + localName 为查询键
    static std::string makeKey(const std::string& namespaceUri, const std::string& localName);

    std::unordered_map<std::string, CreatorFn> m_creators;
};

}  // namespace oso