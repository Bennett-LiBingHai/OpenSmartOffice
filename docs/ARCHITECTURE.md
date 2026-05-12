# OpenSmartOffice — 项目架构书

## 1. 分层架构

```
┌─────────────────────────────────────────────────┐
│  Application Layer                               │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────┐ │
│  │   CLI    │ │ GUI (Qt) │ │  Service (HTTP)   │ │
│  │ 入口     │ │ 入口     │ │  入口            │ │
│  └────┬─────┘ └────┬─────┘ └────────┬─────────┘ │
│       └─────────────┼───────────────┘           │
├─────────────────────┼───────────────────────────┤
│  Core Layer         │                            │
│  ┌──────────────────┴─────────────────────────┐ │
│  │         Document Facade (统一外观)          │ │
│  └──┬─────────────────┬─────────────────┬─────┘ │
│     │ Word Engine     │ Spreadsheet Eng │ PPT   │
│     └───┬─────────────┴────────┬────────┴───┬───│
│         │          DOM Layer   │            │    │
│         └──────────────────────┘            │    │
├─────────────────────────────────────────────┤    │
│  Infrastructure Layer                        │    │
│  ┌──────────┐ ┌──────────┐ ┌─────────────┐ │    │
│  │ OOXML RW │ │ Formula  │ │  Renderer   │ │    │
│  │ (libxml2)│ │ (ixion)  │ │  (Qt Paint) │ │    │
│  └────┬─────┘ └────┬─────┘ └──────┬──────┘ │    │
│       └────────────┼──────────────┘         │    │
├────────────────────┼─────────────────────────┤    │
│  Platform Layer    │                          │    │
│  ┌──────────┐ ┌───┴─────┐ ┌──────────────┐ │    │
│  │ Logging  │ │Concurrent│ │  File/Stream  │ │    │
│  │(spdlog)  │ │(QtConc) │ │               │ │    │
│  └──────────┘ └─────────┘ └──────────────┘ │    │
└─────────────────────────────────────────────┘
```

### 1.1 层间依赖规则（强制）

| 规则 | 说明 |
|------|------|
| **向下依赖** | 上层可以依赖下层，下层**禁止**依赖上层 |
| **同层隔离** | 同层模块之间通过接口通信，禁止直接包含对方头文件。例外：Platform 层内 `base` 为基础类型库，可被 `logging`/`concurrent`/`io` 直接依赖 |
| **跨层穿透禁止** | Application 层不能直接访问 Infrastructure 层（必须经过 Core） |
| **Platform 独立** | Platform 层不依赖任何项目内模块，仅依赖第三方库 |
| **Infrastructure 无状态** | Infrastructure 层不持有文档状态，只提供纯函数/服务式调用 |

### 1.2 编译器可见性校验

模块间依赖通过 CMake `target_link_libraries` 控制。违反层级依赖规则的链接关系在 CMake 配置期报错。每个模块目录自带一个 `CMakeLists.txt` 校验脚本，检查其所链接目标的归属层级。

---

## 2. 模块清单与职责

### 2.1 Platform Layer — `platform/`

| 模块 | 目录 | 职责 | 对外导出 |
|------|------|------|---------|
| **base** | `platform/base/` | 基础类型：`Result<T>`、`ErrorCode`、`Color`（RGB/ARGB/主题色/索引色）、`Length`（emu/cm/mm/inch/pt/px）、`Rect`/`Size`/`Point`、`PartPath`/`RelationshipId`/`ExternalReference` | `oso_base` |
| **logging** | `platform/logging/` | 日志宏封装（spdlog），支持 JSON 结构化输出 | `oso_logging` |
| **concurrent** | `platform/concurrent/` | `ITaskExecutor` 接口 + Qt 实现 `QtTaskExecutor` | `oso_concurrent` |
| **io** | `platform/io/` | 文件/流抽象：`IFileSystem`、`IMemoryStream` | `oso_io` |

### 2.2 Infrastructure Layer — `infra/`

| 模块 | 目录 | 职责 | 对外导出 |
|------|------|------|---------|
| **ooxml-common** | `infra/ooxml/common/` | ZIP 容器抽象、关系映射、内容类型注册（read/write 共用） | `oso_ooxml_common` |
| **ooxml-read** | `infra/ooxml/read/` | OOXML 读取：XML 解析 → SAX 事件流（不含 ZIP 容器） | `oso_ooxml_read` |
| **ooxml-write** | `infra/ooxml/write/` | OOXML 写出：DOM → XML 序列化 → Zip 打包 | `oso_ooxml_write` |
| **formula** | `infra/formula/` | `IFormulaEngine` 接口 + ixion 适配 | `oso_formula` |
| **render** | `infra/render/` | `IRenderer`/`IRenderContext` 接口 + Qt 离屏实现 | `oso_render` |

### 2.3 Core Layer — `core/`

| 模块 | 目录 | 职责 | 对外导出 |
|------|------|------|---------|
| **dom-common** | `core/dom/common/` | 通用 DOM 节点基类、属性系统、序列化标记 | `oso_dom_common` |
| **dom-word** | `core/dom/word/` | Word 文档 DOM：段落、Run、表格、节 | `oso_dom_word` |
| **dom-sheet** | `core/dom/sheet/` | Excel DOM：工作簿、工作表、单元格、公式链 | `oso_dom_sheet` |
| **dom-slide** | `core/dom/slide/` | PPT DOM：幻灯片、母版、形状、动画 | `oso_dom_slide` |
| **word-engine** | `core/engine/word/` | 文字排版、分页、样式解析、修订合并 | `oso_word_engine` |
| **sheet-engine** | `core/engine/sheet/` | 公式求值调度、单元格依赖图、重算触发 | `oso_sheet_engine` |
| **slide-engine** | `core/engine/slide/` | 幻灯片树遍历、动画时间线计算 | `oso_slide_engine` |
| **ooxml-bridge** | `core/ooxml/` | SAX 事件 → DOM 桥接：`OoxmlSaxHandler` + `ElementFactory` | `oso_ooxml_bridge` |
| **facade** | `core/facade/` | `DocumentFacade`：统一入口，根据文件类型路由到对应 Engine | `oso_facade` |

### 2.4 Application Layer — `app/`

| 模块 | 目录 | 职责 | 对外导出 |
|------|------|------|---------|
| **cli** | `app/cli/` | 命令行解析、模式路由 | `oso_cli` |
| **gui-word** | `app/gui/word/` | Word 桌面编辑器 | `oso_gui_word` |
| **gui-sheet** | `app/gui/sheet/` | Excel 桌面编辑器 | `oso_gui_sheet` |
| **gui-slide** | `app/gui/slide/` | PPT 桌面编辑器 | `oso_gui_slide` |
| **gui-common** | `app/gui/common/` | GUI 共享组件：工具栏、菜单、文档视图基类 | `oso_gui_common` |
| **service** | `app/service/` | HTTP REST API、请求调度、文档会话管理 | `oso_service` |
| **main** | `app/main/` | `main()` 入口：解析 `--mode` 参数，路由到 GUI/CLI/Service | 最终可执行文件 |

### 2.5 第三方依赖聚合 — `third_party/`

```
third_party/
├── CMakeLists.txt          # 统一引入所有第三方库
├── cpp-httplib/            # header-only
├── nlohmann_json/          # header-only
├── ixion/                  # 公式引擎
├── spdlog/                 # header-only
└── googletest/             # 测试框架
```

能通过系统包管理器安装（apt）的都用，`find_package()` 引入。
如果不能就使用FetchContent

---

## 3. 目录结构

```
OpenSmartOffice/
├── CMakeLists.txt              # 顶层构建
├── cmake/                      # CMake 宏和函数
│   ├── OsoModule.cmake         # 添加模块的标准宏
│   ├── OsoCheckDeps.cmake      # 层级依赖校验
│   └── Findlibzip.cmake        # libzip 查找模块
├── docs/                       # 设计文档
│   ├── ARCHITECTURE.md
│   ├── DOM_DESIGN.md
│   ├── OOXML_MAPPING.md
│   └── INTERFACES.md
├── platform/                   # ===== Platform Layer =====
│   ├── CMakeLists.txt
│   ├── base/
│   │   ├── CMakeLists.txt
│   │   ├── include/oso/base/
│   │   │   ├── Result.h
│   │   │   ├── ErrorCode.h
│   │   │   ├── Geometry.h      # Rect, Size, Point
│   │   │   ├── Length.h   
│   │   │   ├── DocPath.h       # RelationshipId, ExternalReference, PartPath
│   │   │   └── Color.h
│   │   └── src/
│   ├── logging/
│   │   ├── CMakeLists.txt
│   │   ├── include/oso/logging/
│   │   │   └── Logger.h
│   │   └── src/
│   ├── concurrent/
│   │   ├── CMakeLists.txt
│   │   ├── include/oso/concurrent/
│   │   │   ├── ITaskExecutor.h
│   │   │   └── QtTaskExecutor.h
│   │   └── src/
│   └── io/
│       ├── CMakeLists.txt
│       ├── include/oso/io/
│       │   ├── IFileSystem.h
│       │   └── IStream.h
│       └── src/
├── infra/                      # ===== Infrastructure Layer =====
│   ├── CMakeLists.txt
│   ├── ooxml/
│   │   ├── CMakeLists.txt
│   │   ├── common/                     # ← NEW: shared ZIP/relationship/content-type
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/oso/ooxml/common/
│   │   │   │   ├── IZipArchive.h
│   │   │   │   ├── LibzipZipArchive.h
│   │   │   │   ├── OoxmlNamespaces.h
│   │   │   │   ├── RelationshipMap.h
│   │   │   │   └── ContentTypeRegistry.h
│   │   │   └── src/
│   │   ├── read/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/oso/ooxml/read/
│   │   │   │   └── IOoxmlReader.h
│   │   │   └── src/
│   │   └── write/
│   │       ├── CMakeLists.txt
│   │       ├── include/oso/ooxml/write/
│   │       │   └── IOoxmlWriter.h
│   │       └── src/
│   ├── formula/
│   │   ├── CMakeLists.txt
│   │   ├── include/oso/formula/
│   │   │   ├── IFormulaEngine.h
│   │   │   └── IxionAdapter.h
│   │   └── src/
│   └── render/
│       ├── CMakeLists.txt
│       ├── include/oso/render/
│       │   ├── IRenderContext.h
│       │   ├── IRenderer.h
│       │   └── QtRenderContext.h
│       └── src/
├── core/                       # ===== Core Layer =====
│   ├── CMakeLists.txt
│   ├── dom/
│   │   ├── CMakeLists.txt
│   │   ├── common/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/oso/dom/common/
│   │   │   │   ├── DomNode.h       # DomElement,DomDocument,DomText
│   │   │   │   ├── Drawing.h       # ← NEW: 通用图片/形状节点 (DrawingML)
│   │   │   │   ├── PropertyBag.h
│   │   │   │   └── Styles.h
│   │   │   └── src/
│   │   ├── word/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/oso/dom/word/
│   │   │   │   ├── Table.h
│   │   │   │   └── WordDocument.h
│   │   │   └── src/
│   │   ├── sheet/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/oso/dom/sheet/
│   │   │   │   ├── Workbook.h
│   │   │   │   ├── Worksheet.h
│   │   │   │   ├── Cell.h
│   │   │   │   ├── CellRange.h
│   │   │   │   ├── Formula.h
│   │   │   │   └── Chart.h
│   │   │   └── src/
│   │   └── slide/
│   │       ├── CMakeLists.txt
│   │       ├── include/oso/dom/slide/
│   │       │   ├── Presentation.h
│   │       │   ├── Slide.h
│   │       │   ├── SlideMaster.h
│   │       │   ├── Shape.h
│   │       │   ├── TextFrame.h
│   │       │   └── Animation.h
│   │       └── src/
│   ├── ooxml/
│   │   ├── CMakeLists.txt
│   │   ├── include/oso/ooxml/
│   │   │   ├── OoxmlSaxHandler.h
│   │   │   └── ElementFactory.h
│   │   └── src/
│   ├── engine/
│   │   ├── CMakeLists.txt
│   │   ├── word/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/oso/engine/word/
│   │   │   │   ├── LayoutEngine.h
│   │   │   │   ├── StyleResolver.h
│   │   │   │   └── RevisionTracker.h
│   │   │   └── src/
│   │   ├── sheet/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/oso/engine/sheet/
│   │   │   │   ├── CalcEngine.h
│   │   │   │   ├── DependencyGraph.h
│   │   │   │   └── RecalcScheduler.h
│   │   │   └── src/
│   │   └── slide/
│   │       ├── CMakeLists.txt
│   │       ├── include/oso/engine/slide/
│   │       │   ├── SlideRenderer.h
│   │       │   └── AnimationTimeline.h
│   │       └── src/
│   └── facade/
│       ├── CMakeLists.txt
│       ├── include/oso/facade/
│       │   └── DocumentFacade.h
│       └── src/
├── app/                        # ===== Application Layer =====
│   ├── CMakeLists.txt
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   └── src/
│   │       └── main.cpp
│   ├── cli/
│   │   ├── CMakeLists.txt
│   │   ├── include/oso/cli/
│   │   │   └── CliOptions.h
│   │   └── src/
│   ├── gui/
│   │   ├── CMakeLists.txt
│   │   ├── common/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/oso/gui/common/
│   │   │   └── src/
│   │   ├── word/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/oso/gui/word/
│   │   │   └── src/
│   │   ├── sheet/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── include/oso/gui/sheet/
│   │   │   └── src/
│   │   └── slide/
│   │       ├── CMakeLists.txt
│   │       ├── include/oso/gui/slide/
│   │       └── src/
│   └── service/
│       ├── CMakeLists.txt
│       ├── include/oso/service/
│       │   ├── IServiceTransport.h   # ← NEW: Service 传输层抽象接口
│       │   ├── HttpServer.h
│       │   ├── DocumentSession.h
│       │   └── ApiHandler.h
│       └── src/
├── third_party/
│   └── CMakeLists.txt
└── tests/                      # ===== Tests =====
    ├── CMakeLists.txt
    ├── platform/
    ├── infra/
    │   ├── CMakeLists.txt
    │   └── ooxml/
    │       ├── CMakeLists.txt
    │       ├── test_ooxml_common.cpp
    │       ├── gen_fixture.py    # 生成 minimal.docx 测试固件
    │       └── data/
    │           └── minimal.docx  # 最小合法 docx 测试文件
    ├── core/
    └── app/
```

---

## 4. 构建系统设计

### 4.1 顶层 `CMakeLists.txt`

```
cmake_minimum_required(VERSION 3.22)
project(OpenSmartOffice LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Build Options ----
option(OSO_BUILD_GUI      "Build Qt desktop GUI"        OFF)
option(OSO_BUILD_SERVICE  "Build HTTP service"           OFF)
option(OSO_BUILD_CLI      "Build CLI tool"               ON)
option(OSO_BUILD_TESTS    "Build unit tests"             OFF)
option(OSO_USE_SYSTEM_QT  "Use system-installed Qt"      ON)
option(OSO_LINK_STATIC    "Statically link dependencies" ON)

# Qt
find_package(Qt6 REQUIRED COMPONENTS Core)
if(OSO_BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)
endif()

# System libs
find_package(LibXml2 REQUIRED)
find_package(libzip REQUIRED MODULE)

# spdlog (header-only logging)
find_package(spdlog REQUIRED)

# nlohmann_json (header-only JSON)
find_package(nlohmann_json REQUIRED)

# cpp-httplib (header-only HTTP)
find_package(httplib REQUIRED)

# ---- FetchContent deps ----
include(FetchContent)
add_subdirectory(third_party)

# ---- Layers (bottom-up) ----
add_subdirectory(platform)
add_subdirectory(infra)
add_subdirectory(core)
if(OSO_BUILD_CLI OR OSO_BUILD_GUI OR OSO_BUILD_SERVICE)
    add_subdirectory(app)
endif()

# ---- Tests ----
if(OSO_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

### 4.2 模块添加宏 `cmake/OsoModule.cmake`

```cmake
# usage: oso_add_module(NAME oso_base DIR platform/base
#                        PUBLIC_DEPS Qt6::Core
#                        PRIVATE_DEPS spdlog::spdlog
#                        LAYER platform)
#
# LAYER 参数仅用于元数据，供 OsoCheckDeps 校验用，不影响编译。
macro(oso_add_module)
    # 解析参数 ...
    # 创建 target: add_library(${NAME} ...)
    # 设置 include 路径: target_include_directories(${NAME} PUBLIC include/)
    # 链接依赖: target_link_libraries(${NAME} PUBLIC ${PUBLIC_DEPS} PRIVATE ${PRIVATE_DEPS})
    # 设定 LAYER 属性: set_target_properties(${NAME} PROPERTIES OSO_LAYER ${LAYER})
endmacro()
```

### 4.3 层级依赖校验 `cmake/OsoCheckDeps.cmake`

在 `app/CMakeLists.txt` 构建完成后运行。遍历所有 oso_ 前缀的 target，提取其 `OSO_LAYER` 属性，检查其 `INTERFACE_LINK_LIBRARIES` 和 `LINK_LIBRARIES` 中是否有跨越层级的反向依赖。

```
Validation rule:
  - platform targets  MAY depend on: (nothing internal)
  - infra targets     MAY depend on: platform
  - core targets      MAY depend on: platform, infra
  - app targets       MAY depend on: platform, infra, core
  - tests targets     MAY depend on: any
```

违反规则时 CMake 报 `FATAL_ERROR`。

---

## 5. 数据流

### 5.1 文档打开流程（以 docx 为例）

```
app/main → CliOptions 解析 "--mode cli --open file.docx"
  → DocumentFacade::open("file.docx")
    → IZipArchive::open("file.docx")  [infra/ooxml/common]
    → 遍历 zip entries:
        → IOoxmlReader::readPart("word/document.xml")  [infra/ooxml/read]
        → WordDocument::fromSaxEvents(events)           [core/dom/word]
        → StyleResolver::resolve(paragraph.style)       [core/engine/word]
    → 返回 wordDocument (shared_ptr<WordDocument>)
  → CLI 输出文档统计信息
```

### 5.2 文档保存流程

```
DocumentFacade::save("output.docx")
  → wordDocument → IOoxmlWriter::writePart(DomRoot)  [infra/ooxml/write]
    → XML 序列化（保留 UnknownElement 原样写出）
  → IZipArchive::create("output.docx")  [infra/ooxml/common]
    → 写入 Content_Types.xml, _rels/.rels, word/document.xml ...
    → 压缩写出
```

### 5.3 Service 模式请求流

```
HTTP POST /api/document/render → JSON{ path: "file.docx", format: "pdf" }
  → ApiHandler::handleRender(req)
    → DocumentSession::acquire("file.docx")  // 复用已加载的文档
    → facade->render(session, RenderFormat::PDF)
      → IRenderer::renderDocument(dom, ctx)
      → QtRenderContext::toPdfBytes()
    → 返回二进制 PDF (Content-Type: application/pdf)
```

### 5.4 公式计算流（xlsx）

```
CellValue SheetEngine::getValue(CellRef ref)
  → DependencyGraph::getOrCompute(ref)  [core/engine/sheet]
    → 遍历 formula 依赖链
    → 对每个需要重算的单元格:
        IFormulaEngine::evaluate(formulaText, context)  [infra/formula]
          → ixion 解析公式文本 → AST → 求值
          → 返回 FormulaResult
    → 缓存结果到 CellCache
    → 返回最终值
```

---

## 6. 扩展点设计

### 6.1 支持新文档格式

1. 实现 `IOoxmlReader`/`IOoxmlWriter` 的新派生类（如 `OdfReader`）
2. 在 `core/dom/` 下新增对应的 DOM 模块
3. 在 `core/engine/` 下新增对应的 Engine
4. 在 `DocumentFacade` 注册新格式的工厂函数

### 6.2 替换渲染后端

1. 实现 `IRenderer` 和 `IRenderContext` 的新派生类（如 `SkiaRenderer`）
2. 在 CMake 中切换链接目标：`oso_render` → `oso_render_skia`
3. Core 层零改动

### 6.3 添加新 Service 协议（gRPC）

1. 定义 proto 文件，生成代码
2. 实现 `IServiceTransport` 接口的 gRPC 版本
3. 在 `app/service/grpc/` 下添加，与 `app/service/http/` 并行
4. 编译选项 `-DOSO_SERVICE_BACKEND=grpc` 切换

### 6.4 添加新 GUI 控件

1. 在 `app/gui/common/` 中添加可复用组件
2. 各模块 GUI（word/sheet/slide）只能依赖 `gui-common`，禁止互相依赖
3. 共享渲染通过 `IRenderer` 接口，不直接依赖 Qt 绘制 API

---

## 7. 并发与线程模型

### 7.1 线程分工

| 线程类型 | 数量 | 职责 |
|----------|------|------|
| Main | 1 | Qt 事件循环（GUI 模式）/ 信号处理（Service 模式） |
| TaskPool | N = CPU 核数 | 文档解析、渲染、公式计算等 CPU 密集任务 |
| IO | 2 | 文件读写（Service 模式下由 cpp-httplib 管理） |

### 7.2 线程安全等级

| 组件 | 等级 | 策略 |
|------|------|------|
| DOM 节点 | **非线程安全** | 同一文档只在单一线程操作，通过 `ITaskExecutor` 序列化 |
| DocumentSession | **线程安全** | 内部互斥锁，管理文档的加载/释放 |
| IFormulaEngine | **线程安全** | ixion 本身线程安全，无额外加锁 |
| IRenderer | **可重入** | 同一渲染器不可同时渲染多个文档，不同渲染器实例可并行 |
| IOoxmlReader | **非线程安全** | 每个线程创建自己的 Reader 实例 |

### 7.3 文档并发操作策略

```
// ✅ 正确：不同文档并行处理
executor->submit([] { facade->open("a.docx") });  // 线程 1
executor->submit([] { facade->open("b.docx") });  // 线程 2

// ✅ 正确：同一文档串行操作
auto session = sessionManager->acquire("a.docx");  // 获得独占锁
executor->submit([session] { session->render(); }); // 排队执行

// ❌ 禁止：同一文档跨线程直接操作
// DOM 节点不可跨线程传递裸指针
```

---

## 8. 错误处理策略

### 8.1 Result<T> 模式

```cpp
// platform/base/include/oso/base/Result.h
template <typename T>
class Result {
public:
    static Result<T> ok(T value);
    static Result<T> err(ErrorCode code, std::string message);

    bool isOk() const;
    bool isErr() const;
    T& value();                    // isErr 时抛异常
    const ErrorCode& error() const;
    const std::string& message() const;

private:
    // ...
};
```

### 8.2 错误传播规则

| 层级 | 策略 |
|------|------|
| Platform / Infra | 所有可失败操作返回 `Result<T>`，不抛异常 |
| Core | 使用 `Result<T>` 传递错误，内部可 `assert` 校验不变量 |
| App (CLI) | 转为错误码 + stderr 输出 |
| App (Service) | 转为 HTTP 状态码 + JSON error body |
| App (GUI) | 弹出错误对话框 |

### 8.3 ErrorCode 命名空间

```
ErrorCode 采用分层编码：
  - 0x0000_0000 ~ 0x0FFF_FFFF: Platform 层错误
  - 0x1000_0000 ~ 0x1FFF_FFFF: Infra 层错误
  - 0x2000_0000 ~ 0x2FFF_FFFF: Core 层错误

子模块占用高 16 位中的 4 位：
  ErrorCode::OOXML::ZipCorrupted    = 0x1001_0001
  ErrorCode::Formula::SyntaxError   = 0x1002_0001
  ErrorCode::Render::FontNotFound   = 0x1003_0001
  ErrorCode::DOM::InvalidNodeType   = 0x2001_0001
```
