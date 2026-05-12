# OpenSmartOffice — 抽象接口定义

本文档定义所有跨模块边界的抽象接口。每个接口：
- 名称以 `I` 开头
- 纯虚方法（或仅含 non-virtual 辅助方法）
- 不持有数据成员
- 由 Infrastructure / Platform 层提供，Core 层消费

---

## 1. 接口索引

| 接口 | 所属层 | 所属模块 | 职责 | 实现类（阶段一） |
|------|--------|---------|------|-----------------|
| `ITaskExecutor` | Platform | concurrent | 异步任务调度 | `QtTaskExecutor` |
| `IFileSystem` | Platform | io | 文件系统抽象 | `PosixFileSystem` |
| `IStream` | Platform | io | 字节流读写 | `MemoryStream`, `FileStream` |
| `IOoxmlReader` | Infra | ooxml/read | SAX 流式 XML 读取 | `Libxml2Reader` |
| `IOoxmlWriter` | Infra | ooxml/write | XML 序列化写出 | `Libxml2Writer` |
| `IZipArchive` | Infra | ooxml/common | ZIP 容器读写 | `ZlibZipArchive` |
| `IOoxmlFactory` | Infra | ooxml/common | OOXML 读写组件工厂（Core 层通过此接口创建 Reader/Writer/ZipArchive，不直接依赖具体实现） | `OsoOoxmlFactory` |
| `IFormulaEngine` | Infra | formula | 公式解析和求值 | `IxionEngine` |
| `IFormulaContext` | Infra | formula | 公式求值的上下文：提供单元格值/命名范围解析 | (由 `IxionEngine` 内部创建) |
| `IRenderer` | Infra | render | 文档页面渲染 | `QtRenderer` |
| `IRenderContext` | Infra | render | 渲染目标（位图/PDF） | `QtRenderContext` |
| `IServiceTransport` | App | service | Service 请求/响应调度 | `HttpTransport` |

---

## 2. Platform 层接口

### 2.1 ITaskExecutor

```cpp
// platform/concurrent/include/oso/concurrent/ITaskExecutor.h
#pragma once

#include <functional>
#include <future>
#include <memory>

namespace oso {

enum class TaskPriority {
    Low     = 0,
    Normal  = 1,
    High    = 2,
    Critical = 3,   // 跳过队列，尽量立即执行
};

class ITaskExecutor {
public:
    virtual ~ITaskExecutor() = default;

    // 提交任务，返回 future 用于获取结果或等待完成
    virtual std::future<void> submit(
        std::function<void()> task,
        TaskPriority priority = TaskPriority::Normal) = 0;

    // 与 submit 同，但返回带结果的 future
    template <typename F>
    auto submitWithResult(F&& task, TaskPriority priority = TaskPriority::Normal)
        -> std::future<decltype(task())>;

    // 等待所有已提交任务完成
    virtual void waitAll() = 0;

    // 当前排队任务数（用于监控，非精确值）
    virtual size_t pendingCount() const = 0;

    // 线程池大小
    virtual size_t threadCount() const = 0;
};

// submitWithResult 模板实现
template <typename F>
auto ITaskExecutor::submitWithResult(F&& task, TaskPriority priority)
    -> std::future<decltype(task())> {
    using ResultType = decltype(task());
    auto promise = std::make_shared<std::promise<ResultType>>();
    auto future = promise->get_future();

    submit([promise = std::move(promise), task = std::forward<F>(task)]() mutable {
        try {
            if constexpr (std::is_void_v<ResultType>) {
                task();
                promise->set_value();
            } else {
                promise->set_value(task());
            }
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }, priority);

    return future;
}

} // namespace oso
```

**线程安全：** 所有方法线程安全。

**生命周期：** 全局单例或依赖注入。服务关闭前调用 `waitAll()`。

**阶段二替换：** 实现 `PriorityTaskExecutor`（std::thread + 优先级队列），替换时仅链接目标变更。

---

### 2.2 IFileSystem

```cpp
// platform/io/include/oso/io/IFileSystem.h
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace oso {

class IStream;

class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    // 文件存在性
    virtual bool exists(const std::string& path) const = 0;

    // 文件大小（字节）
    virtual Result<uint64_t> fileSize(const std::string& path) const = 0;

    // 打开文件读取
    virtual Result<std::unique_ptr<IStream>> openRead(
        const std::string& path) = 0;

    // 打开/创建文件写入
    virtual Result<std::unique_ptr<IStream>> openWrite(
        const std::string& path) = 0;

    // 删除文件
    virtual Result<void> remove(const std::string& path) = 0;

    // 遍历目录
    virtual Result<std::vector<std::string>> listDirectory(
        const std::string& path) const = 0;

    // 创建目录（含父目录）
    virtual Result<void> createDirectory(const std::string& path) = 0;

    // 临时文件路径
    virtual std::string tempPath() const = 0;
};

} // namespace oso
```

---

### 2.3 IStream

```cpp
// platform/io/include/oso/io/IStream.h
#pragma once

#include <cstdint>
#include <vector>

namespace oso {

class IStream {
public:
    virtual ~IStream() = default;

    // ---- 读取 ----
    // 读取最多 maxLen 字节到缓冲区，返回实际读取字节数
    virtual Result<size_t> read(uint8_t* buffer, size_t maxLen) = 0;
    // 读取全部剩余字节
    virtual Result<std::vector<uint8_t>> readAll() = 0;

    // ---- 写入 ----
    virtual Result<void> write(const uint8_t* data, size_t len) = 0;
    virtual Result<void> writeAll(const std::vector<uint8_t>& data) {
        return write(data.data(), data.size());
    }

    // ---- 定位 ----
    enum class SeekOrigin { Begin, Current, End };
    virtual Result<uint64_t> seek(int64_t offset, SeekOrigin origin) = 0;
    virtual Result<uint64_t> tell() const = 0;

    // ---- 状态 ----
    virtual bool isOpen() const = 0;
    virtual Result<void> flush() = 0;
    virtual Result<void> close() = 0;

    // ---- 大小 ----
    virtual Result<uint64_t> size() const = 0;
};

} // namespace oso
```

---

## 3. Infra 层接口

### 3.1 IOoxmlReader

```cpp
// infra/ooxml/read/include/oso/ooxml/read/IOoxmlReader.h
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace oso {

struct XmlAttribute {
    std::string namespaceUri;
    std::string localName;
    std::string prefix;
    std::string value;
};

class IOoxmlReader {
public:
    virtual ~IOoxmlReader() = default;

    // ---- SAX 回调 ----
    using StartElementFn = std::function<void(
        const std::string& namespaceUri,
        const std::string& localName,
        const std::string& qName,
        const std::vector<XmlAttribute>& attributes)>;

    using EndElementFn = std::function<void(
        const std::string& namespaceUri,
        const std::string& localName,
        const std::string& qName)>;

    using CharactersFn = std::function<void(const std::string& text)>;

    // ---- 解析 ----
    // 从字节缓冲区解析 XML
    virtual Result<void> parse(
        const std::vector<uint8_t>& xmlData,
        StartElementFn onStart,
        EndElementFn onEnd,
        CharactersFn onText) = 0;

    // 从流解析（用于大文件流式处理）
    virtual Result<void> parseStream(
        IStream& stream,
        StartElementFn onStart,
        EndElementFn onEnd,
        CharactersFn onText) = 0;
};

} // namespace oso
```

**线程安全：** 非线程安全。每个线程创建自己的 Reader 实例。

**性能约束：** `parse()` 对 1MB XML 文件的解析时间 < 5ms（阶段一目标）。

**阶段二替换：** 实现自研 `StreamingOoxmlReader`，实现相同接口。零拷贝、SIMD 加速 XML 分词。

---

### 3.2 IOoxmlWriter

```cpp
// infra/ooxml/write/include/oso/ooxml/write/IOoxmlWriter.h
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace oso {

// XmlWriter 是 IOoxmlWriter 提供的流式写出工具
class XmlWriter {
public:
    virtual ~XmlWriter() = default;

    // 开始/结束元素
    virtual void startElement(
        const std::string& namespaceUri,
        const std::string& localName) = 0;

    virtual void startElementWithPrefix(
        const std::string& namespaceUri,
        const std::string& localName,
        const std::string& prefix) = 0;

    virtual void endElement(
        const std::string& namespaceUri,
        const std::string& localName) = 0;

    // 写出属性
    virtual void attribute(
        const std::string& name,
        const std::string& value) = 0;

    virtual void attributeWithNs(
        const std::string& namespaceUri,
        const std::string& localName,
        const std::string& value) = 0;

    // 写出文本
    virtual void text(const std::string& text) = 0;
    virtual void text(const std::u16string& text) = 0;  // 自动 XML 转义

    // 写出原始 XML 片段（用于 UnknownElement 回写）
    virtual void raw(const std::string& xmlFragment) = 0;

    // 命名空间声明
    virtual void namespaceDecl(const std::string& prefix,
                               const std::string& uri) = 0;
};

class IOoxmlWriter {
public:
    virtual ~IOoxmlWriter() = default;

    // 创建 XmlWriter，写入到内存缓冲区
    virtual std::unique_ptr<XmlWriter> createWriter() = 0;

    // 完成写入，返回序列化后的字节
    virtual Result<std::vector<uint8_t>> finalize(
        std::unique_ptr<XmlWriter> writer) = 0;
};

} // namespace oso
```

**设计要点：** `XmlWriter` 是流式接口，DOM 节点调用 `writeXml(writer)` 时按顺序写出，不需要构建完整的 XML 字符串树。UnknownElement 通过 `raw()` 直接注入原始 XML。

---

### 3.3 IZipArchive

```cpp
// infra/ooxml/common/include/oso/ooxml/common/IZipArchive.h
#pragma once

#include <memory>
#include <string>
#include <vector>

namespace oso {

class RelationshipMap;
class ContentTypeRegistry;

struct ZipEntry {
    std::string name;        // 如 "word/document.xml"
    uint64_t compressedSize;
    uint64_t uncompressedSize;
    uint32_t crc32;
    bool isDirectory;
};

class IZipArchive {
public:
    virtual ~IZipArchive() = default;

    // ---- 打开 / 关闭 ----
    virtual Result<void> open(const std::string& path) = 0;
    virtual Result<void> close() = 0;
    virtual bool isOpen() const = 0;

    // ---- 枚举条目 ----
    virtual Result<std::vector<ZipEntry>> entries() const = 0;
    virtual Result<bool> hasEntry(const std::string& name) const = 0;

    // ---- 读取 ----
    // 读取整个条目到内存
    virtual Result<std::vector<uint8_t>> readEntry(
        const std::string& name) = 0;

    // 流式读取条目（大文件场景）
    virtual Result<std::unique_ptr<IStream>> openEntryStream(
        const std::string& name) = 0;

    // ---- 写入（创建/修改 ZIP）----
    virtual Result<void> createNew(const std::string& path) = 0;
    virtual Result<void> writeEntry(
        const std::string& name,
        const std::vector<uint8_t>& data) = 0;

    virtual Result<void> writeEntryFromStream(
        const std::string& name,
        IStream& stream) = 0;

    // ---- 关系管理便捷方法 ----
    virtual Result<std::unique_ptr<RelationshipMap>> readRelationships(
        const std::string& relsPath) = 0;

    virtual Result<void> writeRelationships(
        const std::string& relsPath,
        const RelationshipMap& rels) = 0;

    // ---- 内容类型管理便捷方法 ----
    virtual Result<std::unique_ptr<ContentTypeRegistry>> readContentTypes() = 0;
    virtual Result<void> writeContentTypes(
        const ContentTypeRegistry& registry) = 0;
};

} // namespace oso
```

**线程安全：** 非线程安全。不同线程可同时打开不同的 `IZipArchive` 实例。

**实现：** 阶段一用 `ZlibZipArchive`，基于 zlib + minizip。阶段二如果 ZIP I/O 成为瓶颈，可实现 `ParallelZipArchive`：写入时并行压缩多个条目。

---

### 3.4 IFormulaEngine

```cpp
// infra/formula/include/oso/formula/IFormulaEngine.h
#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace oso {

// 公式求值的结果
using FormulaValue = std::variant<
    std::monostate,     // Empty
    bool,               // Boolean
    double,             // Number
    std::u16string,     // String
    struct FormulaError // Error
>;

struct FormulaError {
    enum class Code {
        Null, Ref, Value, Div0, NA, Name, Num, Spill, Calc
    };
    Code code;
    std::string message;
};

// 公式求值的上下文：提供外部单元格的值
class IFormulaContext {
public:
    virtual ~IFormulaContext() = default;

    // 解析单元格引用，返回其值
    virtual FormulaValue resolveCell(const std::string& ref) = 0;

    // 解析命名范围
    virtual FormulaValue resolveName(const std::string& name) = 0;

    // 解析范围引用，返回区域内的值列表
    virtual std::vector<std::vector<FormulaValue>> resolveRange(
        const std::string& range) = 0;
};

// 不透明公式句柄 — 由 IFormulaEngine 管理，RAII 自动释放
class ParsedFormula {
public:
    virtual ~ParsedFormula() = default;
    // 不透明类型，具体实现由引擎内部定义
};

class IFormulaEngine {
public:
    virtual ~IFormulaEngine() = default;

    // ---- 解析 ----
    // 解析公式文本，返回类型安全的句柄（RAII 自动释放）
    virtual Result<std::unique_ptr<ParsedFormula>> parse(
        const std::u16string& formulaText) = 0;

    // ---- 求值 ----
    // 对已解析的公式求值
    virtual FormulaValue evaluate(
        const ParsedFormula& parsedFormula,
        IFormulaContext& context) = 0;

    // 解析并求值（便捷方法，内部调用 parse + evaluate）
    virtual FormulaValue evaluate(
        const std::u16string& formulaText,
        IFormulaContext& context) = 0;

    // ---- 依赖分析 ----
    // 返回公式引用的所有单元格引用
    virtual std::vector<std::string> extractReferences(
        const std::u16string& formulaText) = 0;
};

} // namespace oso
```

**线程安全：** 线程安全。`parse()` 和 `evaluate()` 可被多个线程并发调用，引擎内部无共享可变状态。`unique_ptr<ParsedFormula>` 自动管理句柄生命周期，无需手动释放。

**阶段一实现：** `IxionEngine`，将 `FormulaValue` 和 `IFormulaContext` 适配到 ixion 的类型系统。

**阶段二替换：** `NativeFormulaEngine`，自研实现。接口不变，仅切换链接目标。

---

### 3.5 IRenderer / IRenderContext

```cpp
// infra/render/include/oso/render/IRenderer.h
#pragma once

#include <memory>
#include <string>

namespace oso {

class DomNode;
class DomDocument;
class PropertyBag;

// 渲染目标抽象 — 无论输出到什么格式，接口一致
class IRenderContext {
public:
    virtual ~IRenderContext() = default;

    // 页面尺寸（单位：点 / 1/72 inch）
    virtual float pageWidth() const = 0;
    virtual float pageHeight() const = 0;

    // 当前裁剪区域
    virtual void setClipRect(float x, float y, float w, float h) = 0;
    virtual void resetClip() = 0;

    // ---- 绘制原语 ----
    // 文字
    virtual void drawText(
        const std::u16string& text,
        float x, float y,
        const PropertyBag& fontProps) = 0;

    // 带宽度限制的文字（自动换行）
    virtual float drawTextBox(
        const std::u16string& text,
        float x, float y,
        float width, float height,
        const PropertyBag& fontProps,
        const PropertyBag& paraProps) = 0;

    // 矩形
    virtual void fillRect(float x, float y, float w, float h,
                          const PropertyBag& fillProps) = 0;
    virtual void drawRect(float x, float y, float w, float h,
                          const PropertyBag& borderProps) = 0;

    // 线条
    virtual void drawLine(float x1, float y1, float x2, float y2,
                          const PropertyBag& lineProps) = 0;

    // 图片
    virtual void drawImage(const std::vector<uint8_t>& imageData,
                           float x, float y, float w, float h) = 0;

    // ---- 页面控制 ----
    virtual void newPage() = 0;

    // ---- 获取结果 ----
    // 将渲染结果输出为 PDF 字节
    virtual std::vector<uint8_t> toPdfBytes() = 0;

    // 将渲染结果输出为图片
    virtual std::vector<uint8_t> toPngBytes(int dpi = 150) = 0;
};

// 文档渲染器 — 知道如何遍历 DOM 树并绘制到 IRenderContext
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // 渲染整个文档
    virtual Result<void> renderDocument(
        const DomNode* documentRoot,
        IRenderContext& context) = 0;

    // 渲染单个节点（用于局部刷新）
    virtual Result<void> renderNode(
        const DomNode* node,
        IRenderContext& context) = 0;

    // 测量文字宽度（不实际绘制）
    virtual float measureTextWidth(
        const std::u16string& text,
        const PropertyBag& fontProps) = 0;

    // 测量文字高度（给定宽度下自动换行后的高度）
    virtual float measureTextHeight(
        const std::u16string& text,
        float maxWidth,
        const PropertyBag& fontProps,
        const PropertyBag& paraProps) = 0;
};

// 渲染器工厂 — 根据文档类型创建对应的 Renderer
enum class DocumentType { Word, Spreadsheet, Presentation };

class IRendererFactory {
public:
    virtual ~IRendererFactory() = default;
    virtual std::unique_ptr<IRenderer> createRenderer(
        DocumentType type) = 0;
};

} // namespace oso
```

**设计备注：** 当前 `IRenderContext` 同时承担绘制原语和格式输出两种职责。未来如需支持更多输出格式（SVG、EMF 等），可将输出方法（`toPdfBytes()`、`toPngBytes()`）提取到独立的 `IRenderOutput` 接口，`IRenderContext` 仅保留绘制原语。当前为简化 MVP 阶段暂不拆分。

**线程安全：** 可重入。同一 `IRenderer` 实例不可并发使用，但不同实例可并行渲染不同文档。

**阶段一实现：** `QtRenderer` + `QtRenderContext`（QImage + QPainter 离屏）。

**阶段二替换：** `SkiaRenderer` + `SkiaRenderContext`，替换时仅 `infra/render/` 内变更。

---

## 4. App 层接口

### 4.1 IServiceTransport

```cpp
// app/service/include/oso/service/IServiceTransport.h
#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace oso {

struct ServiceRequest {
    std::string method;                     // HTTP 方法 或 gRPC 方法名
    std::string path;                       // URL 路径 或 gRPC service/method
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;
};

struct ServiceResponse {
    int statusCode = 200;                   // HTTP 状态码 或 gRPC 状态码
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;
};

using RequestHandler = std::function<ServiceResponse(const ServiceRequest&)>;

class IServiceTransport {
public:
    virtual ~IServiceTransport() = default;

    // 注册路由处理函数
    virtual void addRoute(
        const std::string& method,
        const std::string& path,
        RequestHandler handler) = 0;

    // 启动服务（绑定端口，开始接受请求）
    virtual Result<void> start(const std::string& host, uint16_t port) = 0;

    // 优雅关闭
    virtual Result<void> shutdown() = 0;

    // 是否正在运行
    virtual bool isRunning() const = 0;
};

} // namespace oso
```

**阶段一实现：** `HttpTransport`，内部用 cpp-httplib。`addRoute` 映射到 HTTP method + path。请求/响应体是 JSON（nlohmann/json 解析）。

**阶段二实现：** `GrpcTransport`，实现相同接口。`addRoute` 映射到 gRPC method。请求/响应体是 protobuf 序列化。

**并行暴露：** 阶段二可同时实例化 `HttpTransport` 和 `GrpcTransport`，两者监听不同端口，共用同一套 `RequestHandler`（handler 内部根据 `headers["Content-Type"]` 判断格式）。

---

## 5. 接口版本控制策略

### 5.1 稳定性分类

| 标记 | 含义 |
|------|------|
| `@stable` | 接口已冻结，添加新方法允许，修改已有方法签名禁止 |
| `@evolving` | 接口可能增加方法，但已有方法不会修改。实现类需跟进 |
| `@internal` | 仅供同层模块使用，外部不应依赖 |

```cpp
/**
 * @stable
 * 线程安全的异步任务执行器。
 * 自 v1.0 起接口冻结。
 */
class ITaskExecutor { ... };

/**
 * @evolving
 * 渲染器接口。当新增渲染输出格式（如 SVG）时可能增加方法。
 */
class IRenderContext { ... };
```

### 5.2 接口兼容性规则

| 变更类型 | 是否允许 | 处理方式 |
|---------|---------|---------|
| 添加纯虚方法 | `@evolving` 接口允许 | 所有实现类同步添加 |
| 添加纯虚方法 | `@stable` 接口禁止 | 新建 `IXXX2` 接口继承原接口 |
| 修改方法签名 | 禁止 | 添加新方法，旧方法标记 deprecated |
| 修改返回值类型 | 禁止 | 同上 |
| 添加非虚辅助方法 | 允许 | 不影响实现类 |
| 修改注释/文档 | 允许 | 不改变 API 契约 |

---

## 6. 接口自检机制

```cpp
// 每个接口在 CMake 配置期生成版本号
// 版本号不匹配时触发编译警告

#define OSO_INTERFACE_VERSION(iface, major, minor) \
    static constexpr int k##iface##VersionMajor = major; \
    static constexpr int k##iface##VersionMinor = minor;

class ITaskExecutor {
    OSO_INTERFACE_VERSION(ITaskExecutor, 1, 0)
    // ...
};

// CMake 校验脚本：
// 对比 infra/ 导出的头文件版本号和 core/ 期望的版本号
// 不匹配 → 编译失败，提示更新实现类
```

---

## 7. 接口使用范例

### 7.1 DocumentFacade 如何使用各接口（M1 核心代码路径）

```cpp
// core/facade/src/DocumentFacade.cpp (伪代码)

// OOXML 工厂接口 — 由 App 层注入具体实现，Core 层不直接依赖 Infra 实现类
class IOoxmlFactory {
public:
    virtual ~IOoxmlFactory() = default;
    virtual std::unique_ptr<IZipArchive> createZipArchive() = 0;
    virtual std::unique_ptr<IOoxmlReader> createReader() = 0;
    virtual std::unique_ptr<IOoxmlWriter> createWriter() = 0;
};

class DocumentFacade {
public:
    DocumentFacade(
        std::shared_ptr<IOoxmlFactory> ooxmlFactory,
        std::shared_ptr<ITaskExecutor> executor,
        std::shared_ptr<IRendererFactory> rendererFactory)
        : m_ooxmlFactory(std::move(ooxmlFactory))
        , m_executor(std::move(executor))
        , m_rendererFactory(std::move(rendererFactory))
    {}

    Result<std::unique_ptr<DomNode>> open(const std::string& path) {
        // 1. 打开 ZIP
        auto zip = m_ooxmlFactory->createZipArchive();
        OSO_TRY(zip->open(path));

        // 2. 读取 Content_Types
        auto ctRegistry = OSO_TRY(zip->readContentTypes());

        // 3. 确定文档类型
        auto docType = detectDocumentType(*ctRegistry);

        // 4. 读取主文档部件
        auto mainPart = getMainPartPath(docType);
        auto xmlData = OSO_TRY(zip->readEntry(mainPart));

        // 5. 解析 XML → DOM
        auto reader = m_ooxmlFactory->createReader();
        auto factory = ElementFactory::forType(docType);
        auto handler = std::make_unique<OoxmlSaxHandler>(factory.get());

        OSO_TRY(reader->parse(xmlData,
            [&](auto ns, auto name, auto qn, auto attrs) {
                handler->startElement(ns, name, attrs);
            },
            [&](auto ns, auto name, auto qn) {
                handler->endElement(ns, name);
            },
            [&](auto text) {
                handler->characters(text);
            }
        ));

        // 6. 返回 DOM 根节点（同时持有 zip 引用以按需读取图片）
        return handler->releaseDocument();
    }

    Result<std::vector<uint8_t>> render(const DomNode* doc, RenderFormat fmt) {
        auto docType = detectType(doc);
        auto renderer = m_rendererFactory->createRenderer(docType);
        auto context = std::make_unique<QtRenderContext>();

        OSO_TRY(renderer->renderDocument(doc, *context));

        switch (fmt) {
            case RenderFormat::PDF: return context->toPdfBytes();
            case RenderFormat::PNG: return context->toPngBytes();
        }
    }

    Result<void> save(const DomNode* doc, const std::string& path) {
        // 1. 创建 ZIP
        auto zip = m_ooxmlFactory->createZipArchive();
        OSO_TRY(zip->createNew(path));

        // 2. 写出主文档部件
        auto writer = m_ooxmlFactory->createWriter();
        auto xmlWriter = writer->createWriter();
        doc->writeXml(*xmlWriter);
        auto xmlBytes = OSO_TRY(writer->finalize(std::move(xmlWriter)));
        OSO_TRY(zip->writeEntry(getMainPartPath(detectType(doc)), xmlBytes));

        // 3. 收集并写出 relationships
        // 4. 写出 content types
        OSO_TRY(zip->close());
        return Result<void>::ok();
    }

private:
    std::shared_ptr<IOoxmlFactory> m_ooxmlFactory;
    std::shared_ptr<ITaskExecutor> m_executor;
    std::shared_ptr<IRendererFactory> m_rendererFactory;
};
```

### 7.2 单元测试中的接口模拟

```cpp
// 测试时可以 mock 任何接口
class MockTaskExecutor : public ITaskExecutor {
public:
    std::future<void> submit(std::function<void()> task, TaskPriority) override {
        // 测试中同步执行，方便断点调试
        task();
        std::promise<void> p;
        p.set_value();
        return p.get_future();
    }
    void waitAll() override {}
    size_t pendingCount() const override { return 0; }
    size_t threadCount() const override { return 1; }
};

class MockFormulaEngine : public IFormulaEngine {
    // 返回预设值，不依赖 ixion
    std::unordered_map<std::string, FormulaValue> m_presetResults;
public:
    void setResult(const std::string& formula, FormulaValue value) {
        m_presetResults[formula] = value;
    }
    FormulaValue evaluate(const std::u16string& formula,
                          IFormulaContext&) override {
        auto it = m_presetResults.find(/* ... */);
        if (it != m_presetResults.end()) return it->second;
        return FormulaValue(std::monostate{});
    }
};
```

---

## 8. 接口文件清单

```
platform/
  concurrent/include/oso/concurrent/ITaskExecutor.h
  io/include/oso/io/IFileSystem.h
  io/include/oso/io/IStream.h

infra/
  ooxml/common/include/oso/ooxml/common/IOoxmlFactory.h
  ooxml/common/include/oso/ooxml/common/IZipArchive.h
  ooxml/common/include/oso/ooxml/common/RelationshipMap.h
  ooxml/common/include/oso/ooxml/common/ContentTypeRegistry.h
  ooxml/read/include/oso/ooxml/read/IOoxmlReader.h
  ooxml/write/include/oso/ooxml/write/IOoxmlWriter.h
  formula/include/oso/formula/IFormulaEngine.h
  formula/include/oso/formula/IFormulaContext.h
  render/include/oso/render/IRenderer.h
  render/include/oso/render/IRenderContext.h

app/
  service/include/oso/service/IServiceTransport.h
```

---

## 9. M1 实现优先级

M1 阶段必须完成的接口实现：

| 优先级 | 接口 | 实现类 | 理由 |
|--------|------|--------|------|
| P0 | `ITaskExecutor` | `QtTaskExecutor` | 所有异步操作的基础 |
| P0 | `IStream` | `MemoryStream` | XML 解析需要字节流输入 |
| P0 | `IOoxmlReader` | `Libxml2Reader` | 读取 docx/xlsx/pptx 的前提 |
| P0 | `IOoxmlWriter` | `Libxml2Writer` | 写出 docx/xlsx/pptx 的前提 |
| P0 | `IZipArchive` | `ZlibZipArchive` | OOXML 本质是 ZIP |
| P1 | `IFileSystem` | `PosixFileSystem` | CLI 模式下文件操作需要 |
| P1 | `IFormulaEngine` | `IxionEngine` | M1 可不集成（可打印占位信息），M3 前必须完成 |
| P1 | `IOoxmlFactory` | `OsoOoxmlFactory` | M1 需要工厂接口以避免 Core 层硬依赖 Infra 具体实现 |
| P2 | `IRenderer` | `QtRenderer` | M2 文字处理 MVP 需要渲染 |
| P2 | `IRenderContext` | `QtRenderContext` | 同上 |
| P3 | `IServiceTransport` | `HttpTransport` | M6 才需要 |
