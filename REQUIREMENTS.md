# OpenSmartOffice — 需求文档

## 1. 项目概述

OpenSmartOffice 是一个基于 C++/Qt 的跨模式办公套件，同时支持桌面 GUI 和无头服务模式，运行于 Linux 平台。核心目标是提供 OOXML 文档的高吞吐处理能力。

## 2. 功能范围

### 2.1 模块组成

| 模块 | 核心功能 |
|------|---------|
| **文字处理 (Word)** | 文档创建/编辑/渲染、段落/表格/图片/页眉页脚、样式管理、修订追踪、邮件合并 |
| **电子表格 (Excel)** | 工作表管理、公式计算引擎、图表生成、数据排序/筛选、数据透视表、条件格式 |
| **演示文稿 (PPT)** | 幻灯片创建/编辑、母版/版式管理、动画/过渡效果、演讲者备注、幻灯片放映 |

### 2.2 运行模式

| 模式 | 说明 |
|------|------|
| **桌面 GUI 模式** | 完整的 Qt 图形界面，供终端用户交互式编辑文档 |
| **无头 CLI 模式** | 命令行调用，用于文档转换、批量处理、自动化生成 |
| **无头 Service 模式** | 常驻进程，通过 HTTP/gRPC API 提供服务，用于高吞吐场景 |

三种模式共享同一套文档处理内核，无头模式不链接 GUI 相关 Qt 模块。

## 3. 文件格式

- 主要格式：OOXML（ISO/IEC 29500）
  - `.docx` — Word 文档
  - `.xlsx` — Excel 工作簿
  - `.pptx` — PowerPoint 演示文稿
- 导出格式（最低支持）：
  - PDF（文字处理和演示文稿）
  - CSV（电子表格）
  - HTML（全部三个模块）

## 4. 性能目标

| 指标 | 目标值 |
|------|--------|
| 文档解析吞吐 | ≥ 200 docx/s（单机，中等复杂度文档） |
| 文档生成吞吐 | ≥ 100 docx/s |
| 公式计算 | 百万单元格公式链求值 < 500ms |
| 内存占用 | 单个 10MB 文档加载后内存增量 < 80MB |
| 并发 | Service 模式支持 ≥ 16 并发请求 |

## 5. 技术架构约束

| 项目 | 选择 |
|------|------|
| 语言 | C++20 |
| UI 框架 | Qt 6.x |
| 构建系统 | CMake |
| 部署形式 | 单体可执行文件（内置静态链接） |
| 平台 | Linux（Ubuntu 22.04+, Debian 12+, RHEL 9+） |
| OOXML 解析 | libxml2 + zlib 封装（阶段一）；自研流式解析器（阶段二，需更高吞吐时） |
| 渲染 | Qt QPA offscreen plugin 离屏渲染（GUI/无头共用渲染管线） |
| 公式引擎 | 集成 ixion（阶段一）；自研引擎（阶段二，需极致并行计算时） |
| Service API | REST / JSON（阶段一）；gRPC（阶段二，序列化成本成为瓶颈时） |
| 并发模型 | Qt Concurrent / QThreadPool（GUI/无头共用） |

## 6. 模块化结构

项目采用严格 4 层架构，层间依赖规则、构建校验、数据流详见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

```
OpenSmartOffice/
├── CMakeLists.txt                   # 顶层构建，定义 OSO_BUILD_* 选项
├── cmake/                           # CMake 宏和校验脚本
│   ├── OsoModule.cmake              # 模块添加宏（统一 target 创建/链接/属性）
│   └── OsoCheckDeps.cmake           # 层间依赖校验（app→core→infra→platform）
├── docs/                            # 设计文档
│   ├── ARCHITECTURE.md              # 分层架构、数据流、扩展点
│   ├── DOM_DESIGN.md                # DOM 基类设计、属性系统、生命周期
│   ├── OOXML_MAPPING.md             # OOXML Schema → C++ 类型映射表
│   └── INTERFACES.md                # 全部抽象接口清单
├── platform/                        # ===== Platform Layer =====
│   ├── base/                        # 基础类型：ErrorCode, Result<T>, Color, Geometry, Length, DocPath
│   ├── logging/                     # 日志封装（spdlog），支持 JSON 结构化输出
│   ├── concurrent/                  # ITaskExecutor 接口 + QtTaskExecutor 实现
│   └── io/                          # 文件/流抽象：IFileSystem, IStream
├── infra/                           # ===== Infrastructure Layer =====
│   ├── ooxml/common/                # OOXML 共用：Zip 容器抽象、关系映射、内容类型注册
│   ├── ooxml/read/                  # OOXML 读取：XML 解析 → SAX 事件流
│   ├── ooxml/write/                 # OOXML 写出：XML 序列化 → Zip 压缩打包
│   ├── formula/                     # IFormulaEngine 接口 + ixion 适配器
│   └── render/                      # IRenderer / IRenderContext 接口 + Qt 离屏实现
├── core/                            # ===== Core Layer =====
│   ├── dom/common/                  # 通用 DOM 基类：DomNode, DomDocument, DomText, PropertyBag
│   ├── dom/word/                    # Word DOM：WordDocument, Paragraph, Run, Table, Section
│   ├── dom/sheet/                   # Excel DOM：Workbook, Worksheet, Cell, Formula, Chart
│   ├── dom/slide/                   # PPT DOM：Presentation, Slide, SlideMaster, Shape, Animation
│   ├── engine/word/                 # 文字排版引擎：分页、样式解析、修订合并
│   ├── engine/sheet/                # 公式调度引擎：依赖图、重算触发、缓存
│   ├── engine/slide/                # 幻灯片引擎：树遍历、动画时间线
│   ├── ooxml/                       # SAX→DOM 桥接：OoxmlSaxHandler, ElementFactory
│   └── facade/                      # DocumentFacade：统一入口，按文件类型路由到对应 Engine
├── app/                             # ===== Application Layer =====
│   ├── main/                        # main() 入口：解析参数，路由 GUI/CLI/Service
│   ├── cli/                         # 命令行模式：参数解析、文档转换/信息/提取
│   ├── gui/common/                  # GUI 共享组件：工具栏、菜单栏、文档视图基类
│   ├── gui/word/                    # Word 桌面编辑器
│   ├── gui/sheet/                   # Excel 桌面编辑器
│   ├── gui/slide/                   # PPT 桌面编辑器
│   └── service/                     # HTTP REST API：请求调度、文档会话管理
└── tests/                           # ===== Tests =====
    ├── platform/
    ├── infra/
    └── core/
```

**关键设计决策：**
- `platform/` 是唯一不依赖项目内其他模块的层，仅依赖 Qt/libxml2/zlib/spdlog 等第三方库
- `infra/` 无状态——不持有文档数据，只提供纯函数/服务式调用；`infra/formula/` 和 `infra/render/` 的接口为阶段二替换实现预留
- `core/dom/` 与 `core/engine/` 分离：DOM 负责数据模型和序列化，Engine 负责排版/计算/渲染等 CPU 密集操作
- `app/` 下三个入口（CLI/GUI/Service）共享同一套 core + infra + platform，互不依赖
- `third_party/` 统一管理第三方库（spdlog, cpp-httplib, nlohmann/json, ixion, googletest，Qt，libxml2，zlib）

## 7. 设计决策

每个决策分两阶段：阶段一优先开发速度、快速出 MVP；阶段二为后期追求更高性能预留升级路径。

---

### 7.1 公式引擎 — 集成 ixion（阶段一）→ 自研引擎（阶段二）

| | 阶段一 | 阶段二 |
|------|--------|--------|
| **方案** | 集成 [ixion](https://gitlab.com/ixion/ixion)（Apache 2.0） | 自研公式引擎 |
| **开发量** | 集成一套 C++ API 封装，约 2 周 | 词法/语法解析、AST 求值、数百内置函数、类型系统，6 人月+ |
| **性能** | 已支持线程级并行计算、依赖图遍历；百万单元格公式链求值 < 500ms 可达 | 可针对自有 DOM 做零拷贝计算、CPU 亲和性绑定、GPU 加速函数求值 |

**阶段一理由：** ixion 是 LibreOffice 核心开发者维护的电子表格公式引擎，C++ 实现，性能生产级。自研及格水平的公式引擎工作量巨大，且无用户可见的差异化价值。

**升级触发条件：**
- 公式计算进入火焰图 Top 3 热点
- 需要 ixion 不支持的数组运算或动态数组语义
- 需要在 GPU 上并行执行特定函数

**升级接口预留：** `infra/formula/` 中公式引擎通过 `IFormulaEngine` 抽象接口访问，`core/engine/sheet/` 通过接口调度计算，编译期可切换 infra 实现。

---

### 7.2 OOXML 解析 — libxml2 封装（阶段一）→ 自研流式解析器（阶段二）

| | 阶段一 | 阶段二 |
|------|--------|--------|
| **方案** | libxml2 + zlib，SAX 流式读取 | 自研 OOXML 专用解析器 |
| **开发量** | OOXML Schema 映射层，约 1—2 月 | Stream 分词器 + 零拷贝反序列化 + Schema 代码生成器，4 人月+ |
| **性能** | 中等复杂度 docx 解析 < 10ms；200 docx/s 吞吐可达 | 跳过热元素、零拷贝解析、并行解压部件，预计 2—5x 提升 |

**阶段一理由：** 成熟稳定的 XML 解析栈，SAX 模式内存可控。开发量集中在 OOXML Schema 的 C++ 类型映射，而非 XML 基础设施。

**升级触发条件：**
- 解析成为吞吐瓶颈（火焰图 > 20%）
- 需要解析极端尺寸文档（> 500MB 的 xlsx）时内存不够
- 并发场景下 libxml2 的锁竞争成为瓶颈

**升级接口预留：** `infra/ooxml/read/` 和 `infra/ooxml/write/` 中 XML 解析器通过 `IOoxmlReader` 和 `IOoxmlWriter` 接口访问；`infra/ooxml/common/` 中 Zip 容器读写通过 `IZipArchive` 抽象；均可编译期替换；`core/ooxml/` 中 SAX→DOM 桥接层与 infra 实现无关。

---

### 7.3 离屏渲染 — Qt offscreen plugin（阶段一）→ 纯 CPU 自绘管线（阶段二）

| | 阶段一 | 阶段二 |
|------|--------|--------|
| **方案** | Qt QPA offscreen plugin，QImage/QPixmap 绘制 | FreeType + HarfBuzz + Skia/AGG 自绘管线 |
| **开发量** | 零。`QT_QPA_PLATFORM=offscreen` 一行环境变量；字体/布局/图片编解码全复用 Qt | 集成多个渲染库、手写排版引擎（尤其中日韩/bidi）、PDF 导出管线，3 人月+ |
| **性能** | 共享 Qt 渲染代码，GUI 和离屏结果一致；内存约等于 GUI 模式减窗口开销 | 极致裁剪，可做到 Qt 方案的 1/5—1/10 内存；绘制性能针对文档场景定向优化 |

**阶段一理由：** GUI 和无头模式共用同一套文字排版、单元格渲染、图表绘制代码，无需任何适配工作。当你在桌面模式修一个渲染 bug 时，Service 模式自动受益。

**升级触发条件：**
- 需要部署到 < 256MB 内存的边缘设备
- 离屏渲染成为吞吐瓶颈且火焰图显示 Qt 绘制栈占比 > 30%
- 有专项资源投入渲染管线极致优化

**升级接口预留：** 渲染通过 `IRenderContext`（位图输出）和 `IRenderer`（文档对象绘制）接口访问，接口定义在 `infra/render/`，Qt 离屏实现在同模块内，可整体替换为 Skia/AGG 实现。

---

### 7.4 Service API — REST（阶段一）→ gRPC（阶段二）

| | 阶段一 | 阶段二 |
|------|--------|--------|
| **方案** | cpp-httplib + nlohmann/json，HTTP/JSON | gRPC + protobuf |
| **开发量** | 几个小时搭建，REST 风格无代码生成步骤 | proto 文件定义 + 代码生成 + gRPC 线程模型与 Qt 事件循环集成 |
| **性能** | JSON 序列化 / 反序列化在文档处理场景中占总延迟 < 5% | protobuf 比 JSON 快 3—10x，流式传输适合大文档 |

**阶段一理由：** 文档处理的瓶颈在 CPU（解析/渲染/公式），不在网络序列化。curl 即可调试。开发速度快一个量级。

**升级触发条件：**
- 序列化/反序列化进入火焰图 > 5%
- 大文档（> 100MB）上传/下载场景变多，需要 protobuf 流式传输
- 调用方要求强类型契约

**升级接口预留：** Service 层通过 `IServiceTransport` 接口定义请求/响应调度，HTTP 实现在 `app/service/http/`，后续可加 `app/service/grpc/` 并行暴露两种 API。

---

### 7.5 并发模型 — Qt Concurrent（阶段一）→ 手写线程池（阶段二）

| | 阶段一 | 阶段二 |
|------|--------|--------|
| **方案** | Qt Concurrent + QThreadPool，`QtConcurrent::run()` | std::thread + 手写任务队列（优先级分级、CPU 亲和性） |
| **开发量** | 一行调用即可异步执行；QFuture/QPromise 现成 | 任务队列、线程池、优雅关闭、结果回传、调试工具链，~1000 行 |
| **性能** | 底层是 pthread，与手写无本质差异；work stealing 开箱即用 | 可对文档处理请求做优先级队列、CPU 核绑定，极端并发下延迟更可控 |

**阶段一理由：** GUI 和无头模式共用同一套并发代码，零分支。QThreadPool 在 Linux 上包装 pthread，性能无损耗。

**升级触发条件：**
- Service 模式下 P99 延迟抖动过高，QThreadPool 的全局队列成为竞争点
- 需要按文档大小对请求进行分级调度（小文档优先、大文档隔离）
- 需要 NUMA 感知的线程绑定

**升级接口预留：** 所有异步任务通过 `ITaskExecutor::submit(task, priority)` 调度，Qt 实现 `QtTaskExecutor` 可直接替换为自研 `PriorityTaskExecutor`。

## 8. 里程碑建议

### 里程碑依赖关系

```
M1 (基础框架)
├── M2 (Word DOM) ──┬── M3 (Word GUI) ──────┐
│                    └── M4 (CLI+导出) ──────┤
├── M5 (Sheet DOM) ──┬── M6 (Sheet GUI+导出) ┤
│                     └───────────────────────┤
├── M7 (PPT DOM) ────┬── M8 (PPT GUI+导出) ──┤
│                     └───────────────────────┤
└─────────────────────→ M9 (Service) → M10 (性能优化) → M11 (集成)
                      M12/M13/M14 (高级功能，可与 M9-M11 并行)
                      M15 (质量保障)
```

| 里程碑 | 前置依赖 | 可并行于 |
|--------|---------|---------|
| M1 | — | — |
| M2 | M1 | M5, M7 |
| M3 | M2 | M4, M6, M8 |
| M4 | M2 | M3, M6, M8 |
| M5 | M1 | M2, M7 |
| M6 | M5 | M3, M4, M8 |
| M7 | M1 (M2 中的 Paragraph/Run/Drawing 共享组件) | M2, M5 |
| M8 | M7 | M3, M4, M6 |
| M9 | M2+M5+M7 (三模块 DOM 就绪) | M12, M13, M14 |
| M10 | M9 (Service 模式提供性能测试入口) | M12, M13, M14 |
| M11 | M3+M6+M8 (三模块 GUI 就绪) | M10, M12, M13, M14 |
| M12 | M3 (Word GUI 就绪) | M13, M14 |
| M13 | M6 (Sheet GUI 就绪) | M12, M14 |
| M14 | M8 (PPT GUI 就绪) | M12, M13 |
| M15 | M11 (集成测试通过) | — |

---

### M1 — 基础框架

**目标：** 项目骨架就绪，OOXML Zip 容器可读可写，空白 docx 可完整 round-trip。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 1.1 | 项目骨架 | 按四层建立目录树：`platform/`(base/logging/concurrent/io)、`infra/`(ooxml/common, ooxml/read, ooxml/write, formula, render)、`core/`(dom/common, dom/word, dom/sheet, dom/slide, engine/word, engine/sheet, engine/slide, ooxml, facade)、`app/`(main, cli, gui/common, gui/word, gui/sheet, gui/slide, service)；顶层 `CMakeLists.txt` 定义 `OSO_BUILD_GUI/SERVICE/CLI/TESTS` 四个 option；`cmake/OsoModule.cmake` 模块添加宏（统一 target 创建/链接/层级属性）；`cmake/OsoCheckDeps.cmake` 层间依赖校验脚本；C++20 标准、编译警告全开、Google C++ Style Guide 的 `.clang-format` | 全量编译通过（含各 option 组合），无警告 |
| 1.2 | 基础类型库 | `platform/base/`：长度单位（emu/cm/mm/inch/pt/px 及互转）；颜色（RGB/ARGB/主题色/索引色）；几何基础（2D 点/矩形/尺寸）；`Result<T>` 和 `ErrorCode` 分层错误类型；文档路径（部件路径、关系 ID、外部引用） | 单元测试覆盖 |
| 1.3 | 日志系统 | `platform/logging/`：基于 spdlog 封装；五级（debug/info/warn/error/fatal）；支持输出到控制台和文件（Service 模式下额外支持 syslog）；JSON 结构化格式开关（为 M6 Service 预留） | `LOG(INFO) << "msg"` 各级别输出正常 |
| 1.4 | OOXML Zip 容器读 | `infra/ooxml/common/`：`IZipArchive` 抽象接口；LibzipZipArchive 实现：打开 `.docx`/`.xlsx`/`.pptx`（均为 Zip），枚举部件条目，按需解压到内存 `std::vector<uint8_t>`；ContentTypeRegistry：`[Content_Types].xml` 解析 → Default/Override → MIME type 映射；RelationshipMap：`_rels/.rels` 解析 → 关系 ID → Target URI 映射 | 打开真实 docx 文件，列举全部部件及 MIME 类型 |
| 1.5 | OOXML Zip 容器写 | `infra/ooxml/common/`：Zip 写出：扩展 `IZipArchive` 接口支持创建/写入，给定部件列表 + MIME 类型 + 关系，写出合法 OOXML Zip；自动生成 `[Content_Types].xml` 和 `_rels/.rels`；写入顺序符合 OOXML 规范（先 Content_Types，再 rels，最后部件） | 生成文件被 MS Word / LibreOffice 成功打开（空白文档） |
| 1.6 | XML 解析基础设施 | `infra/ooxml/read/`：`IOoxmlReader` 抽象接口（编译期可替换实现）；Libxml2Reader：libxml2 SAX 流式读取封装，startElement / endElement / characters 回调 → C++ 类型化事件；XML 命名空间管理（`w:` `r:` `wp:` `a:` `rPr:` 等前缀 → 完整 URI） | 解析任意 OOXML 部件，元素/属性/文本事件序列正确 |
| 1.7 | XML 写出基础设施 | `infra/ooxml/write/`：`IOoxmlWriter` 抽象接口；Libxml2Writer：WriteStartElement / WriteAttribute / WriteText / WriteEndElement；自动命名空间前缀管理；XML 声明和独立属性（`standalone="yes"`）；XML 转义（`& < > " '`） | 写出 XML 经 xmllint 验证 well-formed |
| 1.8 | SAX→DOM 桥接 | `core/ooxml/`：`OoxmlSaxHandler` 接收 infra 层 SAX 事件流 → 构建 DOM 树；`ElementFactory` 根据 OOXML 元素名创建对应 DOM 对象；`core/facade/DocumentFacade` 统一入口：打开文档类型检测 → 路由到对应 DOM 加载器；空白文档 round-trip：打开最小合法 docx → 解析所有部件 XML → 内存 DOM → 写出新 docx；二进制对比原始和输出的 Zip 条目列表（允许 XML 缩进差异，不允许条目缺失/多余） | round-trip 后文件结构一致，可被 Word 打开且无错误提示 |
| 1.9 | 测试框架 | GoogleTest 集成；覆盖率工具（gcov/lcov 或 gcovr）；每个库 target 配 `_test` 可执行文件 | `ctest` 运行全部测试并输出覆盖率摘要 |
| 1.10 | CI 基础流水线 | GitHub Actions 或 Jenkins：ubuntu-22.04 / g++-12；cmake → build → ctest；clang-format 检查；clang-tidy 静态分析 | PR 自动触发，红绿状态可见 |
| 1.11 | 流与文件抽象 | `platform/io/`：`IStream`（读写/定位/尺寸）和 `IFileSystem`（打开/存在检查/枚举目录）抽象接口；`MemoryStream` 内存流实现（供 Zip 部件读写和测试使用） | 单元测试通过 MemoryStream 读写往返 |
| 1.12 | 并发基础设施 | `platform/concurrent/`：`ITaskExecutor` 抽象接口（`submit(task, priority)` / `waitAll`）；`QtTaskExecutor` 基于 QThreadPool 的实现；为 M5 性能优化和 M6 Service 并发提供统一调度层 | 提交 10 个任务并行执行，全部完成后取回结果 |

**里程碑验收：** 程序打开任意空白 `.docx`，打印内部部件清单，写出新 `.docx`，MS Word / LibreOffice 可打开且无错误。

---

### M2 — 文字处理 DOM 与数据层

**目标：** Word 文档的完整 DOM 建模、样式系统、所有结构元素可读写，round-trip 无损。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 2.1 | Word DOM — 段落与文字 | `Document` → `Body` → `Paragraph` → `Run` → `Text` 对象模型；段落属性（对齐、缩进 left/right/firstLine/hanging、段前/段后间距、行间距/固定行距、首行缩进、孤行控制）；文字属性（字号/字体/粗体/斜体/下划线/删除线/上标下标/字间距/颜色/高亮）；字符间距和文本效果（大纲阴影等保留不解构，无损读写） | 含以上属性的 10 段落文档 round-trip 无属性丢失 |
| 2.2 | 样式系统 | `styles.xml` 读写：段落样式、字符样式、表格样式、列表样式；样式继承链（basedOn）解析与合并；默认样式（Normal / DefaultParagraphFont）；样式 ID / 名称双向查找；运行时样式解析（`Pargraph::effectiveStyle()` 合并直接格式+样式继承链） | 样式继承链的文档 reopen 后 `effectiveStyle()` 结果一致 |
| 2.3 | 节（Section） | 节属性：页面尺寸/方向（横/纵）、页边距、分栏、页码起始；节分隔（下一页/连续/奇数页/偶数页）；页眉/页脚引用（default/first/even 三种类型） | 多节文档 round-trip，节属性不丢失 |
| 2.4 | 表格 | `Table` → `TableRow` → `TableCell` → 内嵌段落/嵌套表格；单元格合并（水平/垂直）；表格属性：宽度、对齐、边框、底纹；单元格属性：宽度、垂直对齐、边框、底纹；表格样式应用 | 含合并单元格的 3×4 表格 round-trip 正确 |
| 2.5 | 图片与媒体 | `Drawing` 对象：内嵌图片（`r:drawing` → `wp:inline`）；图片尺寸和位置；图片引用 → `image` 部件关系；支持 png / jpeg / svg；基础图文环绕（四周型/上下型） | 含图片的文档 round-trip，图片不丢失不变形 |
| 2.6 | 页眉与页脚 | 页眉/页脚部件 `header1.xml` `footer1.xml` 独立读写；页眉/页脚中支持段落/文字/图片/表格；页码域（`PAGE` / `NUMPAGES`）；与节属性的关联（2.3 中三种类型） | 含页眉（文字+图片）和页脚（页码）的文档 round-trip 正确 |
| 2.7 | 列表与编号 | 编号定义 `numbering.xml` 读写；有序列表（decimal/upperRoman/lowerLetter 等）；无序列表（bullet 符号/图片）；多级编号及缩进层级；列表与段落的关联（`numPr`） | 三级嵌套编号列表 round-trip，各级符号/缩进正确 |
| 2.8 | 超链接与书签 | 外部超链接（URL）和内部超链接（跳转到书签）；书签定义（`bookmarkStart` / `bookmarkEnd`）；超链接文本样式（默认蓝色下划线） | 含超链接和书签的文档 round-trip 可点击 |
| 2.9 | 基础邮件合并 | 合并域（`MERGEFIELD`）读写；简单数据源（CSV/JSON）绑定；合并域替换为数据源值；合并到新文档 | 模板 + 10 行 CSV → 10 页合并文档 |
| 2.10 | 修订追踪（基础） | 插入/删除修订元素（`ins` / `del`）解析与保留；修订属性（作者/时间）；无损读写（不解构不丢弃）；接受/拒绝单处修订的 DOM 操作 | 含修订标记的文档 round-trip 后修订信息不丢失 |

**里程碑验收：** 含段落/样式/表格/图片/页眉页脚/列表/修订的复杂 Word 文档 round-trip 无损，MS Word 和 LibreOffice 打开均正确。

---

### M3 — 文字处理 GUI

**目标：** Word 桌面编辑器可用，支持格式编辑、表格、图片、撤销重做、剪贴板。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 3.1 | GUI — 文档视图 | `QMainWindow` 主窗口 + 菜单栏/工具栏/状态栏；`QTextEdit` 或 `QGraphicsView` 文档渲染视图；缩放（Ctrl+滚轮）；光标定位和文本选中；滚动性能平滑（100 页文档无明显卡顿） | 打开 docx，内容正确显示，可滚动浏览 |
| 3.2 | GUI — 格式编辑 | 格式工具栏：字体下拉/字号/粗体/斜体/下划线/颜色；段落工具栏：左对齐/居中/右对齐/两端对齐、减少/增加缩进；右键菜单：字体/段落对话框；所见即所得编辑（修改后立即可视） | 选中文字，改字体/颜色/粗体，显示即时更新 |
| 3.3 | GUI — 表格编辑 | 插入表格对话框（行列数）；表格工具栏：插入/删除行列、合并/拆分单元格、边框/底纹；表格内光标导航（Tab 切换单元格）；表格尺寸拖拽调整 | 插入 4×5 表格，合并单元格，输入内容 |
| 3.4 | GUI — 图片插入 | 插入图片对话框（本地文件选择）；图片拖拽调整大小（保持宽高比）；图片裁剪（基础，矩形裁剪）；图文环绕方式选择 | 插入图片、缩放、换环绕方式 |
| 3.5 | GUI — 撤销/重做 | 基于 `QUndoStack` 的命令模式；所有编辑操作封装为 QUndoCommand；撤销/重做工具栏按钮和 Ctrl+Z/Y 快捷键；栈深度可配置（默认 256） | 连续 50 步编辑后撤销到底再重做到顶，文档内容恢复 |
| 3.6 | GUI — 剪贴板操作 | Ctrl+C/V/X 剪贴板操作；跨应用复制粘贴纯文本；内部富文本复制粘贴保留格式；粘贴选项（保留源格式/合并格式/仅文本） | 选中文字 → Ctrl+C → 移动光标 → Ctrl+V 粘贴；从外部浏览器复制文本粘贴到编辑器 |
| 3.7 | GUI — 文件操作 | 新建/打开/保存/另存为文档；最近文件列表；文件拖放打开（从文件管理器拖入）；自动保存（定时保存临时副本，崩溃恢复） | 新建空白文档 → 编辑 → 保存 → 关闭 → 重新打开内容一致 |
| 3.8 | GUI — 查找与替换 | 查找对话框（Ctrl+F）；替换对话框（Ctrl+H）；查找高亮；正则表达式模式（可选）；全字匹配/区分大小写选项 | 在 20 页文档中查找关键词，全部高亮；批量替换正确 |

**里程碑验收：** 用 GUI 创建含多段落、多字体/样式、表格、图片、页眉页脚的 20 页文档，保存为 docx。MS Word 和 LibreOffice 打开均正确显示。撤销/重做、剪贴板操作正常。

---

### M4 — CLI 与导出

**目标：** Word 文档可通过 CLI 转换为 PDF/HTML/txt，信息提取可用。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 4.1 | CLI — 格式转换 | `--to txt`：提取纯文本（保留段落分隔）；`--to html`：段落/样式/表格 → HTML + CSS；`--to pdf`：调用离屏渲染管线生成 PDF；`--from docx --to pdf input.docx -o output.pdf` 一键转换 | 命令行转换输出在浏览器（html）/ PDF 阅读器中正常显示 |
| 4.2 | CLI — 信息与提取 | `--info`：文档属性（页数/段落数/字数/图片数/创建时间/修改时间）；`--extract-images`：提取全部内嵌图片到指定目录；`--extract-text`：提取纯文本到 stdout（适合管道） | `./oso convert --info report.docx` 输出文档统计信息 |
| 4.3 | 导出 — HTML | 文档 → HTML5：段落/样式/表格/图片（嵌入 base64 或独立文件）；CSS 样式表（内嵌或外链）；页眉/页脚 → HTML header/footer | 浏览器打开 HTML 与桌面 GUI 视觉接近 |
| 4.4 | 导出 — PDF | 基于 Qt offscreen 渲染（`QT_QPA_PLATFORM=offscreen`）生成 PDF；`QPdfWriter` / `QPrinter` 输出矢量 PDF；字体嵌入（至少嵌入文档使用的字体子集）；图片保持原始分辨率；书签 → PDF 书签大纲 | 导出 PDF 在 Adobe Reader / Evince 中打开，页码/文字/图片/表格正确 |

**里程碑验收：** CLI 转换 Word 文档为 PDF/HTML/txt 均可接受，PDF 在 Adobe Reader 中正确显示。

---

### M5 — 电子表格 DOM 与公式

**目标：** Excel 工作簿 DOM 完整建模，公式引擎正确计算，round-trip 无损。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 5.1 | 工作表 DOM | `Workbook` → `Worksheet` → `Row` → `Cell`；单元格引用（A1 / R1C1 表示法互转）；单元格数据类型（number / string / boolean / date / error / inlineStr / sharedString / formula）；工作表属性（名称/可见性/缩放/冻结窗格/网格线）；行高/列宽（默认值 + 逐行逐列覆盖） | 含多类型数据的单工作表 round-trip |
| 5.2 | 共享字符串表 | `SharedStrings` 读写；字符串去重（插入时检查已存在）；索引查找 O(log n)；SST 与 Cell 的引用一致性（写回时重排索引） | 含 1000+ 重复字符串单元格的文档 round-trip，SST 索引正确 |
| 5.3 | 单元格样式 | 字体（名称/大小/粗/斜/颜色/下划线）；填充（图案/渐变/前景色/背景色）；边框（上下左右/对角线/样式/颜色）；数字格式（内置格式 ID + 自定义格式字符串）；样式合并到 `CellXf`（格式 → 字体/填充/边框/数字格式 索引组合 + xfId 继承链） | 含全部样式类型的单元格 round-trip 后样式一致 |
| 5.4 | 公式引擎 — ixion 集成 | `IFormulaEngine` 抽象接口；ixion 的 `model_context` → OOXML DOM 适配器；公式字符串解析（`A1+B1` → ixion AST）；单元格引用 → ixion 依赖图；计算顺序拓扑排序；错误类型映射（`#REF!` `#VALUE!` `#DIV/0!` `#NAME?` `#N/A` `#NUM!` `#NULL!`） | SUM/AVERAGE/IF/VLOOKUP 等 20+ 常用公式计算结果与 Excel 一致 |
| 5.5 | 公式引擎 — 函数支持 | 数学：SUM / SUMIF / SUMIFS / PRODUCT / ROUND / ROUNDUP / ROUNDDOWN / ABS / MOD / POWER / SQRT；统计：AVERAGE / COUNT / COUNTA / COUNTIF / MAX / MIN / MEDIAN / STDEV；逻辑：IF / AND / OR / NOT / IFERROR；查找：VLOOKUP / HLOOKUP / INDEX / MATCH；文本：LEFT / RIGHT / MID / LEN / CONCATENATE / TRIM / UPPER / LOWER；日期：TODAY / NOW / DATE / YEAR / MONTH / DAY | 每个函数至少 1 条测试用例，与 Excel/LibreOffice 结果一致 |
| 5.6 | 公式引擎 — 依赖追踪与重算 | 修改单元格时标记脏（dirty flag）；依赖图边遍历回算受影响的单元格（链式重算，不重算无关单元格）；循环引用检测（A1→B1→A1 类）并报错；异步重算（不阻塞 GUI 线程） | 修改 A1，依赖 A1 的 B1 和依赖 B1 的 C1 都重算，D1（无关）不动 |
| 5.7 | 基础图表 | OOXML Chart 部件读写；图表类型：柱状图/折线图/饼图/散点图；数据系列绑定到单元格区域（`SERIES(sheet!$A$1:$A$10, ...)` 语法）；图表属性：标题/图例/坐标轴标签/颜色；在 GUI 中以 Qt Charts 或 QGraphicsView 自绘渲染 | 选中数据区域 → 插入柱状图 → 保存 → Word/Excel 打开图表显示正确 |

**里程碑验收：** 含数字、文字、日期、公式、图表的多工作表工作簿 round-trip 无损，公式计算结果与 Excel 100% 一致。

---

### M6 — 电子表格 GUI 与导出

**目标：** Excel 桌面编辑器可用，CSV/PDF 导出可用。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 6.1 | GUI — 网格视图 | 自定义 `QTableView` 或 `QAbstractScrollArea` 实现虚拟化网格（仅渲染可见单元格）；列头（A/B/C…）和行头（1/2/3…）；单元格选中（单击/Shift+方向键 区域选中/Ctrl+点击 多选）；冻结窗格；缩放 | 1000 行 × 26 列网格，滚动帧率 ≥ 30 fps |
| 6.2 | GUI — 公式栏与编辑 | 公式栏（fx 输入框）显示当前单元格公式或值；单元格内编辑（双击或 F2 进入编辑模式）；公式引用高亮（编辑公式时对应单元格显示彩色边框）；Tab/Enter/方向键 导航（行为与 Excel 一致）；自动填充（拖拽填充柄，序列/复制/公式调整） | 选中 A1=10, A2=20, A3 输入 =SUM(A1:A2)，公式栏显示公式，A3 显示 30 |
| 6.3 | GUI — 工作表操作 | 工作表标签栏（Sheet1/Sheet2…）；添加/删除/重命名/移动工作表；工作表标签颜色；右键工作表标签上下文菜单 | 新建 3 张工作表，重命名，切换标签显示对应内容 |
| 6.4 | GUI — 格式对话框 | 单元格格式对话框（数字/对齐/字体/边框/填充 标签页）；列宽/行高对话框；条件格式规则管理器 UI（为 M7 预留数据模型入口） | 选中单元格 → 右键 → 设置单元格格式 → 修改各项属性 → 立即生效 |
| 6.5 | GUI — 撤销/重做 | 基于 QUndoStack 的命令模式；单元格编辑操作封装为 QUndoCommand；撤销/重做工具栏按钮和 Ctrl+Z/Y 快捷键 | 修改单元格值/格式后撤销恢复正确 |
| 6.6 | GUI — 剪贴板操作 | 单元格复制/剪切/粘贴；跨工作表粘贴；粘贴选项（值/公式/格式/全部）；从外部粘贴 CSV 文本自动分列 | 选中 A1:B3 → Ctrl+C → 选中 D1 → Ctrl+V 粘贴；从文本编辑器粘贴 CSV 数据自动填入单元格 |
| 6.7 | GUI — 图表编辑 | 插入图表对话框（选择类型和数据区域）；图表类型切换；数据系列编辑；图表属性面板（标题/图例/坐标轴） | 选中数据区域 → 插入柱状图 → 修改为折线图 → 保存后 Excel 打开正确 |
| 6.8 | CLI — 转换与提取 | `--to csv`：每工作表导出为一个 CSV 文件（或单工作表指定 `--sheet`）；编码选项（UTF-8/UTF-8 BOM/GBK）；分隔符选项（逗号/分号/Tab）；`--to html`：工作表 → HTML `<table>`；`--info`：工作表数/行列数/公式单元格数/图表数 | `./oso convert --to csv --sheet Sheet1 data.xlsx -o data.csv` |
| 6.9 | 导出 — PDF | 工作表 → PDF（带网格线/列头行头）；页面设置（纸张大小/方向/缩放/页边距）；打印区域（`Print_Area` 命名范围）；分页符；页眉/页脚（页码/工作表名/日期） | 打印预览 → PDF 导出，格式与 Excel "另存为 PDF" 基本一致 |

**里程碑验收：** GUI 中创建含数字、文字、日期、公式、图表的 3 工作表工作簿。CLI 导出 CSV 在 Excel 中打开列正确。

---

### M7 — 演示文稿 DOM

**目标：** PPT 演示文稿 DOM 完整建模，母版/版式/动画可读写，round-trip 无损。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 7.1 | 演示文稿 DOM | `Presentation` → `Slide`（含 SlideLayout 引用 + SlideMaster 引用）；`Slide` → `ShapeTree` → Shape（文本框/图片/图形）；Shape 属性：位置（offset x/y）、尺寸（extent cx/cy）、旋转、翻转、z-order（渲染顺序）；占位符（Placeholder）类型匹配（标题/正文/图片/日期/页码/页脚） | 含 5 张幻灯片的 pptx round-trip，不丢幻灯片不丢形状 |
| 7.2 | 母版与版式 | `SlideMaster` 读写：母版背景/配色方案（主题色 Dark1/Light1/Accent1-6 等）/字体方案/效果方案；`SlideLayout` 读写：版式占位符定义（标题区/正文区/图片区等）；幻灯片 ↔ 版式 ↔ 母版关联及属性继承（布局占位符 + 母版背景） | 切换幻灯片版式（标题页→标题+内容），占位符布局更新 |
| 7.3 | 文本框 | 文本框内段落/文字与 M2 Word DOM 复用（`Paragraph` / `Run`）；文本框内文字样式；文本框对齐（上中下/左中右）；文本框内边距/自动调整（溢出缩排/不自动调整/调整形状）；竖排文字 | 含标题+正文的幻灯片 round-trip，所有文字属性保留 |
| 7.4 | 图片与形状 | 图片插入（与 M2 Drawing/Image 复用）；几何形状（矩形/圆角矩形/椭圆/三角形/箭头/星形等自选图形）；形状样式（填充/轮廓/阴影/发光/柔化边缘/三维）；连接符（直线/曲线/肘形连接符） | 含图片和 3 种形状的幻灯片 round-trip 正确 |
| 7.5 | 表格（幻灯片内） | 幻灯片内嵌表格（与 M2 Table DOM 复用）；表格样式（PowerPoint 表格样式库） | 幻灯片中 4×3 表格 round-trip |
| 7.6 | 动画（基础） | 进入动画：淡入/飞入/缩放/旋转；强调动画：脉冲/跷跷板/放大缩小；退出动画：淡出/飞出/缩放；动画时序（单击开始/与上一动画同时/上一动画之后）；动画持续时间和延迟；动画触发顺序 | 含动画的幻灯片保存后在 PowerPoint 中播放动画一致 |
| 7.7 | 幻灯片过渡 | 切换效果：淡出/推入/擦除/揭开/覆盖/闪光/随机；过渡时长；过渡触发（单击/自动）；应用到全部幻灯片 | 含过渡的 pptx 在 PowerPoint 幻灯片放映时过渡效果正确 |
| 7.8 | 备注与讲义 | 每张幻灯片独立备注（Notes Slide）；`notesSlide` 部件读写；演讲者备注视图（GUI 中幻灯片下方备注面板） | 含备注的 pptx round-trip 备注内容不丢失 |
| 7.9 | 基础图表 | 幻灯片内嵌基础图表（柱状图/饼图/折线图，与 M5 图表 DOM 复用）；图表数据绑定到单元格区域 | 含图表的幻灯片在 PowerPoint 中打开显示正确 |

**里程碑验收：** 含标题页、内容页、图片页、动画的 10 页演示文稿 round-trip 无损，PowerPoint 打开正确。

---

### M8 — 演示文稿 GUI 与导出

**目标：** PPT 桌面编辑器可用，幻灯片放映正常，PDF/图片导出可用。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 8.1 | GUI — 幻灯片编辑器 | `QMainWindow` 布局：左侧幻灯片缩略图面板、中央幻灯片编辑画布、右侧属性面板；画布：`QGraphicsView` / `QGraphicsScene` 渲染幻灯片；形状选中/移动/缩放/旋转（控制点拖拽）；多选（Shift/Ctrl+点击，框选）；对齐参考线和吸附 | 新建幻灯片 → 拖入文本框 → 输入文字 → 插入图片 → 移动/缩放 |
| 8.2 | GUI — 大纲与排序 | 左侧缩略图列表（拖拽排序幻灯片）；大纲视图（仅显示文字层级结构）；幻灯片右键菜单（新建/复制/删除/隐藏/新增节） | 创建 5 张幻灯片，拖拽排成 3→1→2→5→4 的顺序 |
| 8.3 | GUI — 幻灯片放映 | 全屏幻灯片放映模式（`QKeyEvent` 全屏 + 自绘）；前进/后退（Space/→ 前进，← 后退，Home 第一张，End 最后一张）；黑屏/白屏（B/W 键）；Esc 退出放映；激光笔/笔迹标注（可选）；放映计时器（排练计时） | 全屏放映流畅（≥ 30fps），过渡动画正常，导航键正确 |
| 8.4 | GUI — 撤销/重做 | 基于 QUndoStack 的命令模式；幻灯片/形状操作封装为 QUndoCommand；撤销/重做工具栏按钮和 Ctrl+Z/Y 快捷键 | 删除幻灯片后撤销恢复正确 |
| 8.5 | GUI — 剪贴板操作 | 形状/文本框复制粘贴；跨幻灯片粘贴；幻灯片复制粘贴 | 选中形状 → Ctrl+C → 切换幻灯片 → Ctrl+V 粘贴 |
| 8.6 | CLI — 转换 | `--to pdf`：幻灯片 → 单页/一页多幻灯片 → PDF；`--to html`：幻灯片 → HTML 网页；`--to images`：每张幻灯片导出为 PNG/JPEG（指定分辨率） | `./oso convert --to pdf --slides-per-page 4 deck.pptx` 生成正确 PDF |
| 8.7 | 导出 — PDF | 单幻灯片模式（每页 1 张，全尺寸）；讲义模式（每页 2/3/4/6/9 张可选）；备注页模式（幻灯片+备注上下排列）；可打印、不可编辑 | 导出 PDF 包含所有幻灯片，格式与 PowerPoint "导出 PDF" 基本一致 |

**里程碑验收：** 用 GUI 创建含标题页、内容页、图片页的 10 页演示文稿，设置过渡和动画，全屏放映流程正常。导出 PDF 格式正确。

---

### M9 — Service 模式

**目标：** 文档处理能力以 HTTP API 形式提供，支持并发请求，可 Docker 化部署。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 9.1 | HTTP 服务框架 | cpp-httplib 集成（仅 Service target 链接）；`IServiceTransport` 抽象接口；单线程和线程池两种模式；启动/停止生命周期管理；信号处理（SIGTERM/SIGINT 优雅关闭，等待进行中请求完成） | `./oso service --port 8080` 启动，`curl http://localhost:8080/health` 返回 200 |
| 9.2 | REST API — 文档转换 | `POST /api/v1/convert` — 上传文件 → 返回转换后文件；支持格式：docx↔pdf, xlsx↔csv, pptx↔pdf, docx→html, xlsx→html, pptx→html；请求参数：源格式/目标格式/选项（如 CSV 分隔符/PDF 每页幻灯片数）；大文件流式上传（chunked transfer-encoding） | `curl -F file=@test.docx -F to=pdf http://localhost:8080/api/v1/convert -o out.pdf` |
| 9.3 | REST API — 文档生成 | `POST /api/v1/generate` — JSON 模板数据 + 模板文件 → 填充后文档（邮件合并）；`POST /api/v1/generate/blank` — 空白文档生成（给定类型：docx/xlsx/pptx） | 上传 JSON 数据 + 含合并域的 docx 模板 → 返回已填充 docx |
| 9.4 | REST API — 文档分析 | `POST /api/v1/analyze` — 上传文档 → 返回元数据 JSON（段数/字数/图数/表数/工作表数/幻灯片数等）；`GET /api/v1/formats` — 列出支持的格式和转换路径 | `curl -F file=@report.docx http://localhost:8080/api/v1/analyze` 返回结构化 JSON |
| 9.5 | REST API — 文档操作 | `POST /api/v1/merge` — 合并多个 docx/pptx 到一个文件；`POST /api/v1/extract` — 提取文档中的图片/文本/表格数据（JSON）；`POST /api/v1/validate` — 验证 OOXML 合法性（返回错误列表 JSON） | 上传 3 个 docx → 合并为一个 docx → 内容按顺序拼接 |
| 9.6 | 并发请求处理 | 请求队列 + 线程池调度（复用 `ITaskExecutor` 接口）；请求优先级（小文档优先、分析请求优先于转换请求）；并发上限可配置（`--max-concurrent 16`）；请求超时（`--request-timeout 60`）；优雅降级（过载时返回 503 + Retry-After 头） | 16 并发转换请求均成功返回，无数据交错/损坏 |
| 9.7 | 请求验证与错误处理 | 上传文件格式检测（Magic Number，非扩展名）；文件大小限制（`--max-file-size 100M`）；参数校验（不支持的转换对返回 400 + 错误说明）；统一错误 JSON 格式（`{"error": {"code": "...", "message": "..."}}` + 对应 HTTP 状态码） | 传 pp 文件但声称是 docx → 400 错误，说明实际检测到的格式 |
| 9.8 | 监控与可观测性 | 结构化日志（JSON 格式输出到 stdout/stderr，含 request_id / method / path / status / duration_ms）；`GET /metrics` — Prometheus 格式指标（请求总数/成功/失败/延迟分位值 P50 P90 P99 / 并发数 / 内存）；`GET /health` — 就绪检查（依赖组件状态：临时目录可写 / libxml2 可用） | Prometheus 抓取 `/metrics` 端点，Grafana 可视化 |
| 9.9 | 安全加固 | 上传文件隔离（每次请求独立临时目录，请求结束清理）；路径遍历防护（禁止 Zip 中包含 `../../../etc/passwd` 类条目）；XML 炸弹防护（`libxml2` 展开限制：实体展开深度 ≤ 256，展开后大小 ≤ 100MB）；请求体大小限制；速率限制（基于 IP 的令牌桶，可选开启） | 上传含路径遍历的恶意 Zip → 400 拒绝，不写入任意文件 |
| 9.10 | Docker 化 | `Dockerfile`（多阶段构建：gcc 编译 → 最小运行时镜像 ubuntu:22.04）；`docker-compose.yml`（oso-service + Prometheus + Grafana）；镜像大小 < 200MB（运行时）；非 root 用户运行；HEALTHCHECK 指令 | `docker compose up` → `curl localhost:8080/health` 返回 200 |
| 9.11 | 压力测试 | `wrk` 或自研压测工具对 `/api/v1/convert` 施压；验证：≥ 16 并发无错误、P99 延迟 < 目标值、无内存泄漏（运行 1 小时 RSS 稳定） | 16 并发持续 10 分钟，错误率 0%，内存无持续增长 |

**里程碑验收：** Docker 部署后，使用 wrk 16 并发压测 10 分钟无错误。

---

### M10 — 性能优化

**目标：** 所有性能指标（第 4 节）达标。

| # | 子任务 | 详细内容 | 目标值 |
|---|--------|---------|--------|
| 10.1 | 基准测试框架 | Google Benchmark 集成；标准测试文档集（小 1KB / 中 50KB / 大 10MB / 超大 100MB docx xlsx pptx）；关键路径埋点计时（解析/构建 DOM/写出/渲染/公式计算）；CI 中基准测试运行 + 历史趋势图表（可选） | 每次提交可对比性能变化百分比 |
| 10.2 | 解析吞吐优化 | XML SAX 解析热点分析（perf/VTune）；减少内存分配（arena allocator 批量分配 XML 元素）；字符串驻留（interned strings，属性名/命名空间 URI 只存一份）；小文件优化（≤ 10 个部件的 docx 走快速路径） | ≥ 200 docx/s（中等复杂度） |
| 10.3 | 生成吞吐优化 | Zip 写出：并行压缩多个部件（zlib deflate 无依赖部件可并行）；部件顺序优化（无依赖部件流水线写出）；避免 XML 写出时的字符串拷贝（`std::string_view` → libxml2 writeRaw） | ≥ 100 docx/s |
| 10.4 | 内存优化（解析） | SAX → DOM 映射：延迟加载（仅保留必要元素在内存，非核心部件按需加载）；DOM 共享（多个文档实例共享静态资源：字体表、主题、样式默认值）；图片数据零拷贝（不将图片数据从 Zip 缓冲区复制到堆）；SST / SharedStrings 增量加载 | 10MB docx 加载后内存增量 < 80MB |
| 10.5 | 内存优化（DOM） | 小对象池（Paragraph/Run/Cell 等高频小对象使用对象池）；COW（Copy-on-Write）样式表（多个段落共享同一 `ParagraphProperties`）；DOM 压缩表示（用 `std::variant` 代替虚函数多态减少 vtable 开销） | 1000 段落文档内存 ≤ 2× 原始 XML 大小 |
| 10.6 | 公式计算优化 | ixion 并行计算线程数调优（避免超分 false sharing）；公式缓存（相同输入不重算）；惰性求值（不可见工作表不重算）；宽依赖图并行调度（同一层级无依赖公式并行提交） | 百万单元格公式链求值 < 500ms |
| 10.7 | 渲染优化（GUI） | 虚拟化渲染（仅绘制视口内段落/单元格/形状）；增量绘制（仅重绘脏区域）；渲染结果缓存（`QPixmapCache` 缓存已栅格化的文本块/形状）；离屏线程池渲染（文档预渲染到 `QImage`，GUI 线程仅贴图） | GUI 100 页文档滚动 ≥ 60fps |
| 10.8 | 并发优化 | QThreadPool 线程数 = `std::thread::hardware_concurrency()`；大文档分片并行解析（按部件：document.xml / styles.xml / header1.xml 等并行读）；无锁数据结构（读写锁 `QReadWriteLock` 保护 DOM 读多写少场景） | 16 核机器上 8 文档并行解析吞吐 ≥ 6× 单文档 |
| 10.9 | 性能回归测试 | 每条性能指标对应一个基准测试断言（CI 红线）；性能回归自动标 PR（超过红线 ±5% 时 CI 失败）；`benchmark/` 目录存储历史基准数据（JSON） | 提交时自动检测性能退化并阻塞合并 |
| 10.10 | 启动时间优化 | CLI 模式冷启动优化（延迟加载字体、不初始化 GUI 模块）；Service 模式预热（启动时预加载常用文档模板到内存）；动态库懒加载（`QLibrary` / `dlopen` 按需加载） | CLI `--help` 冷启动 < 100ms |

**里程碑验收：** 全部性能指标经 benchmark 测试达到第 4 节目标值，CI 中有性能红线守卫。

---

### M11 — 跨模块集成

**目标：** 三模块联调通过，跨模块功能正确。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 11.1 | Word 嵌入 Excel 图表 | Word 文档中插入 Excel 图表对象（OLE 对象或 DrawingML Chart）；图表数据与 Excel 工作簿关联；更新数据源时图表同步更新 | Word 中插入柱状图 → 修改数据 → 图表更新 |
| 11.2 | PPT 嵌入 Excel 图表 | PPT 幻灯片中嵌入 Excel 图表（与 M7.9/M5.7 图表 DOM 复用）；图表数据编辑 | PPT 中插入图表 → 修改数据 → PowerPoint 打开图表正确 |
| 11.3 | 跨模块剪贴板 | Word 表格 → Excel 粘贴为单元格数据；Excel 数据 → Word 粘贴为表格；PPT 形状 → Word 粘贴为图片 | 从 Excel 复制区域 → Word 中粘贴为表格 |
| 11.4 | 统一文档打开 | 文件拖放自动检测 docx/xlsx/pptx 类型；GUI 根据类型切换到对应编辑器窗口；CLI/Service 统一入口根据文件类型路由 | 拖入任意格式文件自动打开正确编辑器 |
| 11.5 | 集成测试集 | 三模块交叉场景测试用例 ≥ 50 个；跨模块 round-trip 测试；Service API 对三模块的统一转换测试 | 全部集成测试通过 |

**里程碑验收：** 跨模块操作（嵌入图表、跨应用剪贴板）正常工作，集成测试全部通过。

---

### M12 — Word 高级功能

**目标：** Word 模块的高级功能完整。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 12.1 | 邮件合并（高级） | 合并域高级语法（IF 条件域/NEXT 记录跳转/SET 书签/MERGESEQ 序号）；多数据源支持（JSON/CSV/SQLite/xlsx 工作表）；合并到单文档/合并到多文档（每记录一个文件）；合并预览（GUI 中逐记录预览合并结果）；标签/信封批量生成 | 1000 行数据源 + 复杂条件域模板 → 合并文档正确 |
| 12.2 | 修订追踪（完整） | 格式修订（`rPrChange` / `pPrChange`）解析/显示/接受/拒绝；修订者视图（显示所有修订者及颜色）；修订气球（GUI 中右栏显示修订详情）；接受全部/拒绝全部；修订保护（文档锁定为修订模式，需密码关闭） | 与 Word 间带修订追踪的文档互操作正确 |
| 12.3 | 批注 | 批注部件（`comments.xml`）读写；批注锚点（`commentRangeStart` / `commentRangeEnd`）；批注属性（作者/时间/已解决状态）；GUI 批注面板（侧边栏显示批注列表）；回复批注（线程） | 添加批注 → 保存 → Word 打开批注可见；反之亦然 |
| 12.4 | 目录与索引 | TOC 域解析与生成（标题级别 → 目录项）；页码域（`PAGE` / `NUMPAGES` / `PAGEREF`）；目录样式（点线前导符）；GUI 右键"更新域"；RD 域（引用外部文档目录项，可选） | 生成含目录的文档，Word 打开可更新页码 |
| 12.5 | 表单与内容控件 | 内容控件：纯文本/富文本/日期选择器/下拉列表/复选框/图片；控件属性（占位符文字/锁定内容/锁定删除）；控件数据绑定（XML mapping 到 CustomXml 部件）；设计模式 vs 填写模式（GUI 切换） | 创建含全部控件类型的表单文档，Word 打开可填写 |

**里程碑验收：** 邮件合并 1000 行数据不出错；修订追踪与 Word 互操作正确。

---

### M13 — 电子表格高级功能

**目标：** Excel 模块的高级功能完整。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 13.1 | 数据透视表 | `pivotTable` 部件读写；行/列/值/筛选 四个区域；值字段汇总方式（求和/计数/平均/最大/最小）；数据透视缓存（`pivotCache`）；基础布局和样式；GUI 数据透视表字段列表面板（拖拽字段到四区域） | 原始数据 → 插入数据透视表 → 分组汇总 → Excel 打开透视表一致 |
| 13.2 | 条件格式 | 条件类型：单元格值（大于/小于/介于/等于）/ 文本包含 / 日期 / 重复值 / 公式；格式：字体颜色/填充/数据条/色阶/图标集；多个规则优先级；条件格式范围管理（应用于/扩展） | 含数据条 + 色阶 + 图标集 3 条规则的工作表 round-trip 正确 |
| 13.3 | 排序与筛选 | 多列排序（主要/次要关键字，每列独立升降序）；自动筛选（列头下拉箭头，值筛选/条件筛选）；高级筛选（条件区域，复制到新位置）；表格（ListObject）结构化引用（`Table1[Column1]` 公式语法） | 1000 行数据，3 列排序 + 自动筛选，Excel 一致 |
| 13.4 | 数据验证 | 验证类型：整数/小数/列表/日期/时间/文本长度/自定义公式；输入消息（选中单元格时提示）；错误警告（停止/警告/信息 三种样式）；下拉列表（序列验证 → 单元格内下拉箭头） | 含下拉列表和数据验证的单元格，Excel 打开规则一致 |
| 13.5 | 高级图表 | 新增图表类型：面积图/雷达图/气泡图/股价图/组合图；次坐标轴；趋势线；误差线；数据标签；图表样式和颜色方案（与主题色联动） | 组合图（柱状+折线，次坐标轴）在 Excel 中打开一致 |

**里程碑验收：** 数据透视表/条件格式与 Excel 互操作正确。

---

### M14 — 演示文稿高级功能

**目标：** PPT 模块的高级功能完整。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 14.1 | 高级动画 | 路径动画（直线/曲线/自定义路径）；触发器（单击某形状触发另一形状动画）；动画与声音同步；时间线高级编排（动画窗格拖拽排序+时长调整） | 含路径动画+触发器的幻灯片区 PowerPoint 打开动画一致 |
| 14.2 | 多媒体 | 视频嵌入（mp4/webm）；音频嵌入（mp3/wav）；媒体播放控件（播放/暂停/进度条）；媒体裁剪（开始/结束时间）；媒体跨幻灯片播放；视频缩略图预览 | 含视频的 pptx 在 PowerPoint 中可播放 |
| 14.3 | 图表与 SmartArt | 幻灯片内嵌图表（与 M5 图表 DOM 复用）；SmartArt 图示（流程/层次/循环/关系/矩阵/棱锥图）；SmartArt 文本编辑；SmartArt 颜色/样式 | 含 SmartArt 的幻灯片在 PowerPoint 中打开一致 |
| 14.4 | 节与自定义放映 | 幻灯片节（Section）：命名/折叠/展开/移动节；自定义放映（Custom Show）：从全部幻灯片中选子集，定义播放顺序；演示者视图（双屏：主屏放映，次屏备注+计时器+下一张预览） | 创建 3 节 + 2 个自定义放映，导出文件在 PowerPoint 中一致 |

**里程碑验收：** 含视频的演示文稿在 PowerPoint 中播放正常。

---

### M15 — 质量保障与交付

**目标：** 测试覆盖率和代码质量达标，可正式发布。

| # | 子任务 | 详细内容 | 产出/验证 |
|---|--------|---------|----------|
| 15.1 | 单元测试覆盖率 | 全模块 `_test` target 补充测试用例；覆盖率目标 ≥ 70%（行覆盖）；每个公开 API 至少 1 条正常路径 + 1 条边界/错误路径；CI 中 `lcov` 生成覆盖率报告 + 门禁（< 70% 阻塞合并） | `ctest && lcov --summary` 显示 ≥ 70% |
| 15.2 | 集成测试 | 文档 round-trip 测试集：真实世界 docx/xlsx/pptx 各 ≥ 50 个（含 MS Office / LibreOffice / Google Docs / WPS 生成）；round-trip 后二进制对比（允许 XML 格式差异，不允许语义差异）；跨应用互操作测试（本项目写出 → MS Office / LibreOffice 打开验证） | 测试集全部通过，无回归 |
| 15.3 | 模糊测试 | libFuzzer / AFL++ 对 OOXML 解析器模糊测试；种子语料：真实文档 + 人工构造边界样例；CI 中持续模糊测试（夜间定时运行，发现 crash 自动提单）；XML 炸弹/畸形 Zip/截断文件 等已知攻击向量专项测试 | 模糊测试运行 24h 无 crash |
| 15.4 | 内存安全 | AddressSanitizer + LeakSanitizer 构建变体；UndefinedBehaviorSanitizer；CI 中 ASan/LSan/UBSan 变体运行全部测试；Valgrind memcheck 补充检测（夜间 CI） | Sanitizer 变体全量测试通过，0 内存错误 |
| 15.5 | CI/CD 完善 | 多平台构建矩阵：Ubuntu 22.04 / Ubuntu 24.04 / Debian 12 / RHEL 9；多编译器：g++-12 / g++-13 / clang-16；每个 PR：build + test + lint + coverage + sanitizers；合并到 main：自动构建 Docker 镜像推送到 Registry；夜间：模糊测试 + Valgrind + 性能基准对比 | PR 检查 100% 自动化，夜间报告自动推送 |
| 15.6 | 打包 — Debian/Ubuntu | `debian/` 目录：control / rules / changelog / copyright；`libopensmartoffice-core` + `opensmartoffice-cli` + `opensmartoffice-service` + `opensmartoffice-gui` 分包；依赖声明（Qt 6.x runtime / libxml2 / zlib）；`.deb` 构建 CMake 目标（CPack 或 dh_make） | `apt install ./opensmartoffice-gui_*.deb` 安装后可启动 GUI |
| 15.7 | 打包 — RPM/RHEL | `.spec` 文件；RHEL 9 / Fedora 适配；`cmake --build . --target package` 生成 `.rpm` | `dnf install ./opensmartoffice-*.rpm` |
| 15.8 | 打包 — AppImage/静态 | AppImage（全部依赖打包，单文件运行）；静态链接变体（`-static-libgcc -static-libstdc++` 静态链接运行时，Qt 和 libxml2 仍动态）；Linux 全发行版兼容测试矩阵 | AppImage 在 Ubuntu/Debian/Fedora/Arch 上双击即可运行 |
| 15.9 | 文档 — 开发者文档 | `docs/` 目录 + 静态站点生成器（Doxygen / Sphinx）；架构概述（数据流图 / 模块依赖图）；公开 API 文档（doxygen 注释 → HTML）；贡献指南（`CONTRIBUTING.md`：编码规范/提交信息格式/PR 流程）；本地构建指南 | `docs/` 生成静态站点，README 链接到文档 |
| 15.10 | 文档 — 用户手册 | GUI 模式快速入门（图文教程）；CLI 模式命令参考（`oso convert --help` 完整）；Service API 参考（OpenAPI / Swagger 规范文档）；示例文档库（含典型用例的 docx/xlsx/pptx） | 新用户可在 10 分钟内完成第一个文档转换 |
| 15.11 | 国际化框架 | Qt 翻译系统（`.ts` / `.qm`）；`QObject::tr()` 包裹所有 GUI 可见字符串；`lupdate` / `lrelease` CMake 集成；翻译源语言：英文（en_US）；首版目标语言：简体中文（zh_CN）；语言切换（运行时动态切换菜单语言） | GUI 切换到中文后，全部菜单/对话框/状态栏显示中文 |
| 15.12 | 版本管理与发布 | 语义化版本（semver）：`MAJOR.MINOR.PATCH`；CMake 中版本号单一来源（`project(OpenSmartOffice VERSION x.y.z)`）；`CHANGELOG.md`（Keep a Changelog 格式）；Git 标签触发发布流水线（构建所有包 → 上传到 GitHub Release） | `git tag v1.0.0 && git push --tags` → CI 自动发布 |

**里程碑验收：** 测试覆盖率 ≥ 70%，Sanitizer 全绿，`.deb`/`.rpm`/AppImage 三种包均可安装运行，开发者文档站上线。
## 9. 非功能需求

- 所有模块可独立编译，GUI 和无头模式可分别构建
- 文档对象模型（DOM）需支持无损读写（保留未识别元素）
- 日志分级（debug/info/warn/error），Service 模式支持结构化日志（JSON）
- 单元测试覆盖率 ≥ 70%
- 代码遵循 Google C++ Style Guide
