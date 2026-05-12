#pragma once

#include "oso/ooxml/read/IOoxmlReader.h"

#include <memory>

namespace oso {

/// IOoxmlReader 的 libxml2 实现。
///
/// 基于 libxml2 的 SAX2 接口实现 XML 解析。将 C 风格的回调
/// 桥接到 IOoxmlReader 定义的 C++ 回调上。
///
/// 不可拷贝，可移动。析构函数负责释放 libxml2 资源。
class Libxml2Reader : public IOoxmlReader {
   public:
    /// 构造一个新的 Libxml2Reader 实例。
    Libxml2Reader();
    /// 销毁此实例并释放所有 libxml2 资源。
    ~Libxml2Reader() override;

    // 不可拷贝，可移动。
    Libxml2Reader(const Libxml2Reader&) = delete;
    Libxml2Reader& operator=(const Libxml2Reader&) = delete;
    Libxml2Reader(Libxml2Reader&&) noexcept;
    Libxml2Reader& operator=(Libxml2Reader&&) noexcept;

    /// @copydoc IOoxmlReader::parse
    Result<void> parse(const std::vector<uint8_t>& xmlData, StartElementFn onStart,
                       EndElementFn onEnd, CharactersFn onText) override;

    /// @copydoc IOoxmlReader::parseStream
    Result<void> parseStream(IStream& stream, StartElementFn onStart, EndElementFn onEnd,
                             CharactersFn onText) override;

    struct Impl;

   private:
    std::unique_ptr<Impl> m_impl;  ///< PIMPL 惯用法，隐藏 libxml2 类型和状态。
};

}  // namespace oso
