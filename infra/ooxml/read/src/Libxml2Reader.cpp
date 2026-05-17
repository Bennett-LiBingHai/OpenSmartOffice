#include "oso/ooxml/read/Libxml2Reader.h"

#include "oso/base/ErrorCode.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace oso {

/// 内部状态，持有回调引用和 libxml2 解析上下文。
/// 设计为可重置结构，每次 parse/parseStream 调用时复用。
struct Libxml2Reader::Impl {
    // public members (internal to this file)
    IOoxmlReader::StartElementFn onStart;
    IOoxmlReader::EndElementFn onEnd;
    IOoxmlReader::CharactersFn onText;
    xmlParserCtxtPtr ctxt = nullptr;
    bool error = false;
    std::string errorMessage;

    void reset() {
        onStart = nullptr;
        onEnd = nullptr;
        onText = nullptr;
        if (ctxt != nullptr) {
            xmlFreeParserCtxt(ctxt);
            ctxt = nullptr;
        }
        error = false;
        errorMessage.clear();
    }
};

Libxml2Reader::Libxml2Reader() : m_impl(std::make_unique<Impl>()) {}

Libxml2Reader::~Libxml2Reader() { m_impl->reset(); }

Libxml2Reader::Libxml2Reader(Libxml2Reader&&) noexcept = default;
Libxml2Reader& Libxml2Reader::operator=(Libxml2Reader&&) noexcept = default;

// ============================================================
// SAX2 回调 — libxml2 调用的静态 C 函数，将数据转发给 C++ Impl。
// ============================================================
namespace {

// 识别到<element>时触发
void onStartElementNs(void* ctx, const xmlChar* localname, const xmlChar* prefix,
                      const xmlChar* nsUri, int nbNamespaces, const xmlChar** namespaces,
                      int nbAttributes, int nbDefaulted, const xmlChar** attributes) {
    (void)nbNamespaces;
    (void)namespaces;
    (void)nbDefaulted;

    auto* impl = static_cast<Libxml2Reader::Impl*>(ctx);
    if (!impl->onStart) {
        return;
    }

    std::string nsUriStr = (nsUri != nullptr) ? reinterpret_cast<const char*>(nsUri) : "";
    std::string localStr = (localname != nullptr) ? reinterpret_cast<const char*>(localname) : "";
    std::string prefixStr = (prefix != nullptr) ? reinterpret_cast<const char*>(prefix) : "";

    std::string qName;
    if (!prefixStr.empty()) {
        qName = prefixStr + ":" + localStr;
    } else {
        qName = localStr;
    }

    // libxml2 attributes array: [localname, prefix, nsUri, valueStart, valueEnd] × nbAttributes
    // 实际 SAX2 接口每个属性占 5 个 slot
    std::vector<XmlAttribute> attrs;
    for (int i = 0; i < nbAttributes; ++i) {
        int idx = i * 5;
        XmlAttribute attr;
        if (attributes[idx + 0] != nullptr) {
            // value 以 (start, end) 指针给出
            const xmlChar* valStart = attributes[idx + 3];
            const xmlChar* valEnd = attributes[idx + 4];
            if ((valStart != nullptr) && (valEnd != nullptr) && valEnd > valStart) {
                attr.value.assign(reinterpret_cast<const char*>(valStart),
                                  static_cast<size_t>(valEnd - valStart));
            } else if (valStart != nullptr) {
                attr.value = reinterpret_cast<const char*>(valStart);
            }
        }
        attr.localName = (attributes[idx + 0] != nullptr)
                             ? reinterpret_cast<const char*>(attributes[idx + 0])
                             : "";
        attr.prefix = (attributes[idx + 1] != nullptr)
                          ? reinterpret_cast<const char*>(attributes[idx + 1])
                          : "";
        attr.namespaceUri = (attributes[idx + 2] != nullptr)
                                ? reinterpret_cast<const char*>(attributes[idx + 2])
                                : "";
        attrs.push_back(std::move(attr));
    }

    impl->onStart(nsUriStr, localStr, qName, attrs);
}

// 识别到</element>时触发
void onEndElementNs(void* ctx, const xmlChar* localname, const xmlChar* prefix,
                    const xmlChar* nsUri) {
    auto* impl = static_cast<Libxml2Reader::Impl*>(ctx);
    if (!impl->onEnd) {
        return;
    }

    std::string nsUriStr = (nsUri != nullptr) ? reinterpret_cast<const char*>(nsUri) : "";
    std::string localStr = (localname != nullptr) ? reinterpret_cast<const char*>(localname) : "";
    std::string prefixStr = (prefix != nullptr) ? reinterpret_cast<const char*>(prefix) : "";

    std::string qName;
    if (!prefixStr.empty()) {
        qName = prefixStr + ":" + localStr;
    } else {
        qName = localStr;
    }

    impl->onEnd(nsUriStr, localStr, qName);
}

// 识别到内容时触发
void onCharacters(void* ctx, const xmlChar* ch, int len) {
    auto* impl = static_cast<Libxml2Reader::Impl*>(ctx);
    if (!impl->onText || ch == nullptr || len <= 0) {
        return;
    }
    impl->onText(std::string(reinterpret_cast<const char*>(ch), static_cast<size_t>(len)));
}

// 发生警告时调用
void onWarning(void* ctx, const char* msg, ...) {
    // 警告静默忽略
    (void)ctx;
    (void)msg;
}

// 发生错误时调用
void onError(void* ctx, const char* msg, ...) {
    auto* impl = static_cast<Libxml2Reader::Impl*>(ctx);
    impl->error = true;

    va_list args;
    va_start(args, msg);
    std::array<char, 1024> buf;
    std::vsnprintf(buf.data(), buf.size(), msg, args);
    va_end(args);
    impl->errorMessage = buf.data();
}

// xmlParseDocument开始时调用
int streamReadCallback(void* context, char* buffer, int len) {
    auto* stream = static_cast<IStream*>(context);
    auto result = stream->read(reinterpret_cast<uint8_t*>(buffer), static_cast<size_t>(len));
    if (result.isErr()) {
        return -1;
    }
    return static_cast<int>(result.value());
}

// xmlParseDocument结束时调用
int streamCloseCallback(void* /*context*/) {
    return 0;  // 不负责关闭 IStream
}

}  // namespace

// ============================================================
// parse() — 一次性从内存缓冲区解析完整的 XML，使用 xmlSAXUserParseMemory。
// ============================================================

Result<void> Libxml2Reader::parse(const std::vector<uint8_t>& xmlData, StartElementFn onStart,
                                  EndElementFn onEnd, CharactersFn onText) {
    if (xmlData.empty()) {
        return Result<void>::err(ErrorCode::OOXMLXmlParseError, "Empty XML data");
    }

    m_impl->reset();
    m_impl->onStart = std::move(onStart);
    m_impl->onEnd = std::move(onEnd);
    m_impl->onText = std::move(onText);

    // 初始化 libxml2 解析器（内部计数器保护，多线程安全）
    xmlInitParser();

    // 构造 SAX2 handler
    xmlSAXHandler saxHandler = {};
    std::memset(&saxHandler, 0, sizeof(saxHandler));
    saxHandler.startElementNs = onStartElementNs;
    saxHandler.endElementNs = onEndElementNs;
    saxHandler.characters = onCharacters;
    saxHandler.warning = onWarning;
    saxHandler.error = onError;
    saxHandler.initialized = XML_SAX2_MAGIC;

    int result = xmlSAXUserParseMemory(&saxHandler, m_impl.get(),
                                       reinterpret_cast<const char*>(xmlData.data()),
                                       static_cast<int>(xmlData.size()));

    if (result != 0 || m_impl->error) {
        std::string msg = m_impl->errorMessage.empty() ? "XML parse error" : m_impl->errorMessage;
        return Result<void>::err(ErrorCode::OOXMLXmlParseError, std::move(msg));
    }

    m_impl->reset();

    return Result<void>::ok();
}

// ============================================================
// parseStream() — 从 IStream 流式解析，通过 xmlCreateIOParserCtxt 按需读取数据。
// ============================================================

Result<void> Libxml2Reader::parseStream(IStream& stream, StartElementFn onStart, EndElementFn onEnd,
                                        CharactersFn onText) {
    m_impl->reset();
    m_impl->onStart = std::move(onStart);
    m_impl->onEnd = std::move(onEnd);
    m_impl->onText = std::move(onText);

    xmlInitParser();

    xmlSAXHandler saxHandler = {};
    std::memset(&saxHandler, 0, sizeof(saxHandler));
    saxHandler.startElementNs = onStartElementNs;
    saxHandler.endElementNs = onEndElementNs;
    saxHandler.characters = onCharacters;
    saxHandler.warning = onWarning;
    saxHandler.error = onError;
    saxHandler.initialized = XML_SAX2_MAGIC;

    m_impl->ctxt = xmlCreateIOParserCtxt(&saxHandler, m_impl.get(), streamReadCallback,
                                         streamCloseCallback, &stream, XML_CHAR_ENCODING_UTF8);

    if (m_impl->ctxt == nullptr) {
        m_impl->reset();
        return Result<void>::err(ErrorCode::OOXMLXmlParseError, "Failed to create parser context");
    }

    xmlCtxtUseOptions(m_impl->ctxt, XML_PARSE_NONET);

    int result = xmlParseDocument(m_impl->ctxt);

    if (result != 0 || m_impl->error) {
        std::string errMsg =
            m_impl->errorMessage.empty() ? "XML parse error" : m_impl->errorMessage;
        return Result<void>::err(ErrorCode::OOXMLXmlParseError, std::move(errMsg));
    }

    m_impl->reset();

    return Result<void>::ok();
}

}  // namespace oso
