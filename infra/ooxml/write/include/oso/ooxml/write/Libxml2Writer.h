#pragma once

#include "oso/ooxml/write/IOoxmlWriter.h"
#include <memory>

namespace oso {

/// IOoxmlWriter 的 libxml2 实现。
///
/// 基于 libxml2 的 xmlTextWriter API 实现 XML 写出。
/// 命名空间前缀通过 OoxmlNamespaces::prefixMap() 自动查找，
/// 各命名空间首次使用时自动生成 xmlns 声明。
///
/// 不可拷贝，可移动。析构函数负责释放 libxml2 资源。
class Libxml2Writer : public IOoxmlWriter {
public:
    Libxml2Writer();
    ~Libxml2Writer() override;

    Libxml2Writer(const Libxml2Writer&) = delete;
    Libxml2Writer& operator=(const Libxml2Writer&) = delete;
    Libxml2Writer(Libxml2Writer&&) noexcept;
    Libxml2Writer& operator=(Libxml2Writer&&) noexcept;

    void setOutput(IStream& stream) override;

    Result<void> writeStartDocument(bool standalone = true) override;
    Result<void> writeEndDocument() override;
    Result<void> writeStartElement(
        const std::string& namespaceUri,
        const std::string& localName) override;
    Result<void> writeEndElement() override;
    Result<void> writeAttribute(
        const std::string& namespaceUri,
        const std::string& localName,
        const std::string& value) override;
    Result<void> declareNamespace(const std::string& namespaceUri) override;
    Result<void> writeText(const std::string& text) override;

    struct Impl;
private:
    std::unique_ptr<Impl> m_impl;
};

} // namespace oso
