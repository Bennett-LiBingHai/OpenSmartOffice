#include"oso/ooxml/ElementFactory.h"
#include "oso/ooxml/common/OoxmlNamespaces.h"
#include "oso/dom/word/WordDocument.h"

namespace oso {

// ============================================================
// ElementFactory 实现
// ============================================================

ElementFactory& ElementFactory::instance() {
    static ElementFactory factory;

    // 首次访问时注册所有已知的 OOXML → DOM 映射
    // 只执行一次（static 变量保证线程安全）
    static bool initialized = false;
    if (!initialized) {
        //首次初始化时可能在下一句还未执行时进入多个线程
        initialized = true;

        const auto& wns = OoxmlNamespaces::kWordprocessingML;
        std::string wnsStr(wns);

        // ---- WordprocessingML 元素注册 ----

        // <w:document> → WordDocument
        factory.registerCreator(wnsStr, "document",
            [](const std::string& localName, const std::string& nsUri) -> std::unique_ptr<DomNode> {
                auto doc = std::make_unique<WordDocument>();
                return doc;
            });

        // <w:body> → Body
        factory.registerCreator(wnsStr, "body",
            [](const std::string& localName, const std::string& nsUri) -> std::unique_ptr<DomNode> {
                return std::make_unique<Body>();
            });

        // <w:p> → Paragraph
        factory.registerCreator(wnsStr, "p",
            [](const std::string& localName, const std::string& nsUri) -> std::unique_ptr<DomNode> {
                return std::make_unique<Paragraph>();
            });

        // <w:r> → Run
        factory.registerCreator(wnsStr, "r",
            [](const std::string& localName, const std::string& nsUri) -> std::unique_ptr<DomNode> {
                return std::make_unique<Run>();
            });

        // <w:t> → Text
        factory.registerCreator(wnsStr, "t",
            [](const std::string& localName, const std::string& nsUri) -> std::unique_ptr<DomNode> {
                return std::make_unique<Text>();
            });

        // <w:sectPr> → Section
        factory.registerCreator(wnsStr, "sectPr",
            [](const std::string& localName, const std::string& nsUri) -> std::unique_ptr<DomNode> {
                return std::make_unique<Section>();
            });
    }

    return factory;
}

std::string ElementFactory::makeKey(const std::string& namespaceUri,
                                    const std::string& localName) {
    return namespaceUri + "\x1F" + localName; // 用分隔符避免冲突
}

void ElementFactory::registerCreator(const std::string& namespaceUri,
                                     const std::string& localName,
                                     CreatorFn creator) {
    m_creators[makeKey(namespaceUri, localName)] = std::move(creator);
}

std::unique_ptr<DomNode> ElementFactory::create(const std::string& namespaceUri,
                                                const std::string& localName) const {
    // 检查是否有注册的创建函数
    auto it = m_creators.find(makeKey(namespaceUri, localName));
    if (it != m_creators.end()) {
        return it->second(localName, namespaceUri);
    }

    // 未注册的元素 → 泛型 DomElement（保留全部属性和子元素）
    return std::make_unique<DomElement>(localName, namespaceUri);
}

bool ElementFactory::isRegistered(const std::string& namespaceUri,
                                  const std::string& localName) const {
    return m_creators.find(makeKey(namespaceUri, localName)) != m_creators.end();
}

}