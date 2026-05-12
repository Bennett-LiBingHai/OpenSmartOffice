#include "oso/ooxml/write/Libxml2Writer.h"

#include "oso/base/ErrorCode.h"
#include "oso/ooxml/common/OoxmlNamespaces.h"

#include <libxml/xmlwriter.h>

#include <cstdio>
#include <cstring>
#include <unordered_set>

namespace oso {

// ============================================================
// 输出回调 — 将 libxml2 输出桥接到 IStream
// ============================================================
namespace {

int writeCallback(void* context, const char* buffer, int len) {
    auto* stream = static_cast<IStream*>(context);
    auto result = stream->write(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(len));
    if (result.isErr())
        return -1;
    return len;
}

int closeCallback(void* /*context*/) {
    return 0;  // 不负责关闭流
}

}  // anonymous namespace

// ============================================================
// PIMPL 实现
// ============================================================
struct Libxml2Writer::Impl {
    IStream* stream = nullptr;
    xmlTextWriterPtr writer = nullptr;
    xmlOutputBufferPtr outBuf = nullptr;
    bool hasError = false;
    bool documentStarted = false;
    std::string errorMessage;
    // 跟踪已声明的 URI，避免子元素重复 xmlns
    std::unordered_set<std::string> declaredNs;

    void reset() {
        if (writer) {
            xmlFreeTextWriter(writer);
            writer = nullptr;
        }
        outBuf = nullptr;
        stream = nullptr;
        hasError = false;
        documentStarted = false;
        errorMessage.clear();
        declaredNs.clear();
    }

    /// 查找命名空间 URI 对应的标准 OOXML 前缀。
    std::string findPrefix(const std::string& nsUri) const {
        if (nsUri.empty())
            return {};
        const auto& map = OoxmlNamespaces::prefixMap();
        for (const auto& [prefix, uri] : map) {
            if (uri == nsUri)
                return std::string(prefix);
        }
        return {};
    }

    /// 写出 xmlns:prefix="nsUri"，仅在首次遇到时声明。
    Result<void> declareNamespace(const std::string& nsUri, const std::string& prefix) {
        if (nsUri.empty() || prefix.empty())
            return Result<void>::ok();
        if (declaredNs.count(nsUri))
            return Result<void>::ok();

        std::string attrName = "xmlns:" + prefix;
        int rc =
            xmlTextWriterWriteAttribute(writer, BAD_CAST attrName.c_str(), BAD_CAST nsUri.c_str());
        if (rc < 0) {
            return Result<void>::err(ErrorCode::OOXML_XmlParseError,
                                     "Failed to declare namespace: " + nsUri);
        }
        declaredNs.insert(nsUri);
        return Result<void>::ok();
    }
};

// ============================================================
// 构造/析构/移动
// ============================================================

Libxml2Writer::Libxml2Writer() : m_impl(std::make_unique<Impl>()) {
}

Libxml2Writer::~Libxml2Writer() {
    m_impl->reset();
}

Libxml2Writer::Libxml2Writer(Libxml2Writer&&) noexcept = default;
Libxml2Writer& Libxml2Writer::operator=(Libxml2Writer&&) noexcept = default;

// ============================================================
// setOutput — 创建 xmlTextWriter，绑定到 IStream
// ============================================================

void Libxml2Writer::setOutput(IStream& stream) {
    m_impl->reset();
    m_impl->stream = &stream;

    m_impl->outBuf = xmlOutputBufferCreateIO(writeCallback, closeCallback, &stream,
                                             nullptr  // xmlCharEncodingHandler* — nullptr = UTF-8
    );

    m_impl->writer = xmlNewTextWriter(m_impl->outBuf);
    if (m_impl->writer) {
        xmlTextWriterSetIndent(m_impl->writer, 1);
        xmlTextWriterSetIndentString(m_impl->writer, BAD_CAST "  ");
    }
}

// ============================================================
// writeStartDocument / writeEndDocument
// ============================================================

Result<void> Libxml2Writer::writeStartDocument(bool standalone) {
    if (!m_impl->writer) {
        return Result<void>::err(ErrorCode::OOXML_XmlParseError, "Writer not initialized");
    }

    int rc = xmlTextWriterStartDocument(m_impl->writer,
                                        nullptr,  // version — nullptr = "1.0"
                                        "UTF-8", standalone ? "yes" : nullptr);

    if (rc < 0) {
        m_impl->hasError = true;
        return Result<void>::err(ErrorCode::OOXML_XmlParseError, "Failed to write start document");
    }
    m_impl->documentStarted = true;
    return Result<void>::ok();
}

Result<void> Libxml2Writer::writeEndDocument() {
    if (!m_impl->writer) {
        return Result<void>::err(ErrorCode::OOXML_XmlParseError, "Writer not initialized");
    }

    int rc = xmlTextWriterEndDocument(m_impl->writer);
    m_impl->documentStarted = false;
    if (rc < 0) {
        m_impl->hasError = true;
        return Result<void>::err(ErrorCode::OOXML_XmlParseError, "Failed to write end document");
    }
    return Result<void>::ok();
}

// ============================================================
// writeStartElement / writeEndElement
// ============================================================

Result<void> Libxml2Writer::writeStartElement(const std::string& namespaceUri,
                                              const std::string& localName) {
    if (!m_impl->writer) {
        return Result<void>::err(ErrorCode::OOXML_XmlParseError, "Writer not initialized");
    }
    if (!m_impl->documentStarted) {
        return Result<void>::err(ErrorCode::OOXML_XmlParseError,
                                 "writeStartDocument() must be called before writeStartElement()");
    }

    std::string prefix = m_impl->findPrefix(namespaceUri);
    std::string qName = prefix.empty() ? localName : prefix + ":" + localName;

    int rc = xmlTextWriterStartElement(m_impl->writer, BAD_CAST qName.c_str());
    if (rc < 0) {
        m_impl->hasError = true;
        return Result<void>::err(ErrorCode::OOXML_XmlParseError,
                                 "Failed to write start element: " + localName);
    }

    // 首次遇到此 namespace URI 时，在当前元素上写出 xmlns:prefix="..."
    auto declareResult = m_impl->declareNamespace(namespaceUri, prefix);
    if (declareResult.isErr())
        return declareResult;

    return Result<void>::ok();
}

Result<void> Libxml2Writer::writeEndElement() {
    if (!m_impl->writer) {
        return Result<void>::err(ErrorCode::OOXML_XmlParseError, "Writer not initialized");
    }

    int rc = xmlTextWriterEndElement(m_impl->writer);
    if (rc < 0) {
        m_impl->hasError = true;
        return Result<void>::err(ErrorCode::OOXML_XmlParseError, "Failed to write end element");
    }
    return Result<void>::ok();
}

// ============================================================
// writeAttribute
// ============================================================

Result<void> Libxml2Writer::writeAttribute(const std::string& namespaceUri,
                                           const std::string& localName, const std::string& value) {
    if (!m_impl->writer) {
        return Result<void>::err(ErrorCode::OOXML_XmlParseError, "Writer not initialized");
    }

    int rc = 0;
    if (namespaceUri.empty()) {
        rc = xmlTextWriterWriteAttribute(m_impl->writer, BAD_CAST localName.c_str(),
                                         BAD_CAST value.c_str());
    } else {
        std::string prefix = m_impl->findPrefix(namespaceUri);
        // 确保属性的命名空间已声明
        auto declareResult = m_impl->declareNamespace(namespaceUri, prefix);
        if (declareResult.isErr())
            return declareResult;

        std::string qName = prefix + ":" + localName;
        rc = xmlTextWriterWriteAttribute(m_impl->writer, BAD_CAST qName.c_str(),
                                         BAD_CAST value.c_str());
    }

    if (rc < 0) {
        m_impl->hasError = true;
        return Result<void>::err(ErrorCode::OOXML_XmlParseError,
                                 "Failed to write attribute: " + localName);
    }
    return Result<void>::ok();
}

// ============================================================
// declareNamespace
// ============================================================

Result<void> Libxml2Writer::declareNamespace(const std::string& namespaceUri) {
    if (!m_impl->writer) {
        return Result<void>::err(ErrorCode::OOXML_XmlParseError, "Writer not initialized");
    }
    if (namespaceUri.empty())
        return Result<void>::ok();

    std::string prefix = m_impl->findPrefix(namespaceUri);
    return m_impl->declareNamespace(namespaceUri, prefix);
}

// ============================================================
// writeText
// ============================================================

Result<void> Libxml2Writer::writeText(const std::string& text) {
    if (!m_impl->writer) {
        return Result<void>::err(ErrorCode::OOXML_XmlParseError, "Writer not initialized");
    }
    if (!m_impl->documentStarted) {
        return Result<void>::err(ErrorCode::OOXML_XmlParseError,
                                 "writeStartDocument() must be called before writeText()");
    }

    // 会自动转义
    int rc = xmlTextWriterWriteString(m_impl->writer, BAD_CAST text.c_str());

    if (rc < 0) {
        m_impl->hasError = true;
        return Result<void>::err(ErrorCode::OOXML_XmlParseError, "Failed to write text");
    }
    return Result<void>::ok();
}

}  // namespace oso
