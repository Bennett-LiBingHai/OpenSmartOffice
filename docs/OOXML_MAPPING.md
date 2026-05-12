# OpenSmartOffice — OOXML 映射设计

## 1. OOXML 文件物理结构

OOXML 文件是一个 ZIP 包，包含多个 XML "部件"。

### 1.1 ZIP 内部布局

#### docx
```
Archive.zip
├── [Content_Types].xml              ← 文件类型注册表
├── _rels/.rels                      ← 根关系映射
├── word/
│   ├── document.xml                 ← 主文档体
│   ├── styles.xml                   ← 样式表
│   ├── settings.xml                 ← 文档设置
│   ├── numbering.xml                ← 编号定义
│   ├── fontTable.xml                ← 字体表
│   ├── header1.xml                  ← 页眉
│   ├── footer1.xml                  ← 页脚
│   ├── media/                       ← 图片/视频
│   │   ├── image1.png
│   │   └── image2.jpeg
│   ├── theme/theme1.xml             ← 主题
│   └── _rels/document.xml.rels      ← 文档级关系
├── docProps/
│   ├── core.xml                     ← 核心属性（作者、标题）
│   └── app.xml                      ← 应用属性
└── customXml/                       ← 自定义 XML 数据部件
```

#### xlsx
```
Archive.zip
├── [Content_Types].xml
├── _rels/.rels
├── xl/
│   ├── workbook.xml                 ← 工作簿定义
│   ├── styles.xml                   ← 样式表
│   ├── sharedStrings.xml            ← 共享字符串表
│   ├── theme/theme1.xml
│   ├── worksheets/
│   │   ├── sheet1.xml
│   │   └── sheet2.xml
│   ├── charts/                      ← 图表（自身也是 OOXML）
│   └── _rels/workbook.xml.rels
├── docProps/
│   ├── core.xml
│   └── app.xml
└── xl/pivotCache/                   ← 数据透视表缓存
```

#### pptx
```
Archive.zip
├── [Content_Types].xml
├── _rels/.rels
├── ppt/
│   ├── presentation.xml             ← 演示文稿定义
│   ├── slideMasters/
│   │   ├── slideMaster1.xml
│   │   └── slideLayouts/
│   │       ├── slideLayout1.xml
│   │       └── slideLayout2.xml
│   ├── slides/
│   │   ├── slide1.xml
│   │   └── slide2.xml
│   ├── notesSlides/
│   ├── theme/theme1.xml
│   ├── media/
│   └── _rels/presentation.xml.rels
└── docProps/
```

---

## 2. 命名空间体系

### 2.1 根命名空间前缀映射

```cpp
// Namespace.h
namespace OoxmlNs {
    // 内容类型
    inline constexpr auto ct  = "http://schemas.openxmlformats.org/package/2006/content-types";

    // 关系
    inline constexpr auto rel = "http://schemas.openxmlformats.org/package/2006/relationships";

    // WordprocessingML
    inline constexpr auto w   = "http://schemas.openxmlformats.org/wordprocessingml/2006/main";
    inline constexpr auto wp  = "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing";
    inline constexpr auto wps = "http://schemas.microsoft.com/office/word/2010/wordprocessingShape";
    inline constexpr auto wpg = "http://schemas.microsoft.com/office/word/2010/wordprocessingGroup";
    inline constexpr auto w14 = "http://schemas.microsoft.com/office/word/2010/wordml";

    // SpreadsheetML
    inline constexpr auto s   = "http://schemas.openxmlformats.org/spreadsheetml/2006/main";
    inline constexpr auto x14 = "http://schemas.microsoft.com/office/spreadsheetml/2009/9/main";
    inline constexpr auto xm  = "http://schemas.microsoft.com/office/excel/2006/main";

    // PresentationML
    inline constexpr auto p   = "http://schemas.openxmlformats.org/presentationml/2006/main";
    inline constexpr auto p14 = "http://schemas.microsoft.com/office/powerpoint/2010/main";

    // DrawingML (共享)
    inline constexpr auto a   = "http://schemas.openxmlformats.org/drawingml/2006/main";
    inline constexpr auto pic = "http://schemas.openxmlformats.org/drawingml/2006/picture";
    inline constexpr auto c   = "http://schemas.openxmlformats.org/drawingml/2006/chart";

    // VML (遗留)
    inline constexpr auto v   = "urn:schemas-microsoft-com:vml";
    inline constexpr auto o   = "urn:schemas-microsoft-com:office:office";

    // Office 共享
    inline constexpr auto mc  = "http://schemas.openxmlformats.org/markup-compatibility/2006";
    inline constexpr auto r   = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";
    inline constexpr auto vt  = "http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes";
    inline constexpr auto dc  = "http://purl.org/dc/elements/1.1/";
    inline constexpr auto dcterms = "http://purl.org/dc/terms/";
    inline constexpr auto cp  = "http://schemas.openxmlformats.org/package/2006/metadata/core-properties";
}
```

### 2.2 命名空间 → 模块映射

```cpp
// 每个 OOXML 命名空间映射到一个 DOM 模块
const std::unordered_map<std::string, DomModule> kNsModuleMap = {
    {OoxmlNs::w,   DomModule::Word},
    {OoxmlNs::s,   DomModule::Sheet},
    {OoxmlNs::p,   DomModule::Slide},
    {OoxmlNs::a,   DomModule::Common},    // DrawingML 归入 Common
    {OoxmlNs::pic, DomModule::Common},
    {OoxmlNs::v,   DomModule::Common},    // VML 归入 Common（仅解析保留）
    {OoxmlNs::r,   DomModule::Common},    // Relationships 归入 Common
};

enum class DomModule { Common, Word, Sheet, Slide };
```

**平台工具类：** 编码转换工具 `UtfConverter` 定义在 `platform/base/include/oso/base/UtfConverter.h`，供 SAX→DOM 桥接层和 DOM→XML 写出层共同使用，不属于任何 OOXML 命名空间映射模块。

---

## 3. 映射规则

### 3.1 元素映射规则

| OOXML | C++ DOM | 示例 |
|-------|---------|------|
| `w:p` | `Paragraph` | Word 段落 |
| `w:r` | `Run` | 字符运行 |
| `w:t` | `DomText` | 纯文本 |
| `w:tbl` | `Table` | 表格 |
| `w:tr` | `TableRow` | 表格行 |
| `w:tc` | `TableCell` | 表格单元格 |
| `s:worksheet` | `Worksheet` | Excel 工作表 |
| `s:sheetData` | 不映射（仅容器） | 其子 `s:row → s:c` 映射为 Cell |
| `s:row` | `Row` | 行 |
| `s:c` | `Cell` | 单元格 |
| `p:sp` | `Shape` | 幻灯片形状 |
| `p:txBody` | `TextFrame` | 文本框 |

### 3.2 纯容器元素处理规则

某些 OOXML 元素仅用于分组，不携带独立语义：
- `w:body` → Word DOM 的 `Body` 节点（语义弱，但保留用于结构完整性）
- `w:sdtContent` → 结构化文档标签，映射为 `SdtBlock`（如果实现）或 `UnknownElement`（未实现）
- `s:sheetData` → 不映射，其子节点直接挂到 `Worksheet`
- `mc:AlternateContent` → 解析时选第一个 `mc:Choice` 的 `Requires` 匹配项，或 `mc:Fallback`

**规则：** 纯容器元素在 DOM 中如果不需要独立操作，可以省略——子节点直接挂到容器元素的父节点。如果后期发现需要独立操作（如为一个容器整体设置属性），再引入对应的包装节点。

### 3.3 属性映射规则

| OOXML 属性 | 映射方式 | 示例 |
|-----------|---------|------|
| 简单值属性 | `PropertyBag.set<T>(key, value)` | `w:sz="24"` → `PropertyKeys::Font_Size, 24` |
| 枚举属性 | `PropertyBag.set<std::string>(key, "center")` | `w:jc="center"` → `PropertyKeys::Para_Alignment, "center"` |
| 引用属性 (rId) | 节点内部 `m_relationshipId` | `<a:blip r:embed="rId4">` → `m_relationshipId = "rId4"` |
| 索引属性 | 节点内部字段 | `s:s="1"` (style index) → `Cell::m_styleIndex = 1` |

### 3.4 属性存储位置规则

OOXML 属性映射到 C++ DOM 时，存储位置分为两类：

| 存储位置 | 适用场景 | 示例 |
|---------|---------|------|
| **PropertyBag** | 可继承、可合并的格式属性 | 字体属性（w:rPr）、段落属性（w:pPr）、单元格格式 |
| **节点成员字段** | 身份标识、跨部件引用、索引引用 | rId（关系引用）、CellRef（单元格地址）、styleIndex（样式索引） |

**判断标准：**
- 如果属性需要参与样式继承链（Word）或索引查找（Excel），放入 PropertyBag
- 如果属性是节点身份标识或跨部件引用，作为节点成员字段
- 如果不确定，优先放入 PropertyBag（更灵活）

**与 DOM_DESIGN.md 的对应：** 详见 DOM_DESIGN.md §3.5 "属性系统不适用的场景"。

### 3.5 元素自身无属性、子元素承载属性

OOXML 大量使用子元素承载属性（而非 XML attribute）：

```xml
<!-- 这是 w:b (加粗) — 元素自身存在即表示 true -->
<w:rPr>
  <w:b/>           <!-- 加粗 -->
  <w:i/>           <!-- 斜体 -->
  <w:sz w:val="24"/>  <!-- 字号 12pt → 24 半磅 -->
</w:rPr>
```

映射规则：
- 独立空元素 `w:b/` → `PropertyBag::set<bool>(Font_Bold, true)`
- 有 val 的元素 `w:sz w:val="24"` → `PropertyBag::set<int>(Font_Size, 24)`
- 缺失该元素 → `PropertyBag::get<bool>(Font_Bold)` 返回 `std::nullopt`（表示"使用继承/默认值"）

---

## 4. SAX 到 DOM 的转换流程

### 4.1 总体流程

```
ZipArchive::open(path)
  → 读取 [Content_Types].xml → ContentTypeRegistry
  → 遍历 .rels → RelationshipMap
  → 找到主部件 (document.xml / workbook.xml / presentation.xml)
    → SAX 流式解析：
        startElement(nsUri, localName, attributes)
          → ElementFactory::create(nsUri, localName)
          → 返回 DomNode*
          → 父节点 → appendChild(node)
        characters(text)
          → 当前节点如果是 Run → appendChild(new DomText(text))
        endElement(nsUri, localName)
          → 完成当前节点，回溯到父节点
  → 返回 Document 根节点
```

### 4.2 元素工厂

```cpp
// core/ooxml/include/oso/ooxml/ElementFactory.h
class ElementFactory {
public:
    using CreatorFn = std::function<std::unique_ptr<DomNode>(
        const std::string& nsUri,
        const std::string& localName,
        const std::vector<XmlAttribute>& attributes
    )>;

    // 注册已知元素 → DomNode 的映射
    void registerElement(
        const std::string& nsUri,
        const std::string& localName,
        CreatorFn creator
    );

    // 查找：先精确匹配，失败则创建 UnknownElement
    std::unique_ptr<DomNode> create(
        const std::string& nsUri,
        const std::string& localName,
        const std::vector<XmlAttribute>& attributes
    );

    static ElementFactory& wordFactory();    // 预注册所有 Word 元素
    static ElementFactory& sheetFactory();   // 预注册所有 Excel 元素
    static ElementFactory& slideFactory();   // 预注册所有 PPT 元素
};
```

### 4.3 注册示例（Word）

```cpp
// WordElementRegistry.cpp
void registerWordElements(ElementFactory& f) {
    // 块级元素
    f.registerElement(OoxmlNs::w, "p",     &Paragraph::createFromXml);
    f.registerElement(OoxmlNs::w, "tbl",   &Table::createFromXml);
    f.registerElement(OoxmlNs::w, "sdt",   &UnknownElement::createFromXml);  // 暂未实现

    // 行内元素
    f.registerElement(OoxmlNs::w, "r",     &Run::createFromXml);
    f.registerElement(OoxmlNs::w, "hyperlink", &Hyperlink::createFromXml);
    f.registerElement(OoxmlNs::w, "drawing",  &DomDrawing::createFromXml);

    // 文本
    f.registerElement(OoxmlNs::w, "t",     &DomText::createFromXml);
    f.registerElement(OoxmlNs::w, "delText", &DomText::createFromXml);
}
```

### 4.4 SAX Handler 状态机

```cpp
// core/ooxml/include/oso/ooxml/OoxmlSaxHandler.h
class OoxmlSaxHandler {
public:
    void startElement(const std::string& nsUri,
                      const std::string& localName,
                      const std::vector<XmlAttribute>& attributes);

    void characters(const std::string& text);

    void endElement(const std::string& nsUri,
                    const std::string& localName);

    std::unique_ptr<DomNode> releaseDocument();

private:
    ElementFactory* m_factory;               // 当前文档类型的工厂
    std::stack<DomNode*> m_openNodes;        // 未闭合的节点栈
    DomNode* m_currentNode = nullptr;        // 正在接收字符数据的节点
    std::unique_ptr<DomNode> m_document;     // 解析完成后的根节点
    std::string m_textBuffer;               // 字符数据缓冲区
};
```

### 4.5 编码转换

OOXML 文件内部使用 UTF-8 编码，但 DOM 节点（如 `DomText`）内部使用 `std::u16string`（UTF-16）存储。

**转换位置：** `OoxmlSaxHandler::characters()` 回调中。

```cpp
void OoxmlSaxHandler::characters(const std::string& text) {
    // IOoxmlReader 回调传入 UTF-8 文本
    // 转换为 UTF-16 后构造 DomText
    auto utf16 = UtfConverter::utf8ToUtf16(text);
    auto textNode = std::make_unique<DomText>(std::move(utf16));
    // 挂载到当前节点
    m_currentNode->appendChild(std::move(textNode));
}
```

**工具类：** `platform/base/UtfConverter` 提供 `utf8ToUtf16()` / `utf16ToUtf8()` 静态方法，内部使用 `QString::fromUtf8()` / `QString::toUtf8()` 或 `std::codecvt<char8_t, char16_t>` 实现。

**写出时反向转换：** `DomText::writeXml()` 中调用 `UtfConverter::utf16ToUtf8(m_text)` 将 UTF-16 文本转回 UTF-8 后交给 `XmlWriter::text()`。

---

## 5. DOM 到 XML 的写出流程

### 5.1 总体流程

```
DocumentFacade::save(path)
  → 遍历 DOM 树，为每个节点调用 writeXml()
  → 一级节点 → 一级 XML 文件 (如 word/document.xml)
  → 收集所有 relationship 引用 → 生成 .rels 文件
  → 收集所有部件类型 → 生成 [Content_Types].xml
  → ZipArchive::create(path) → 逐个写入条目
```

### 5.2 writeXml 实现模式

```cpp
// 每个 DomNode 子类实现自己的 writeXml
void Paragraph::writeXml(XmlWriter& writer) const {
    writer.startElement(OoxmlNs::w, "p");

    // 写出属性子元素
    writeProperties(writer);

    // 递归写出所有子节点
    for (size_t i = 0; i < childCount(); ++i) {
        childAt(i)->writeXml(writer);
    }

    writer.endElement(OoxmlNs::w, "p");
}

void Run::writeProperties(XmlWriter& writer) const {
    if (m_runProperties.getValueCount() == 0) return;

    writer.startElement(OoxmlNs::w, "rPr");

    // 写已知属性
    for (auto& entry : m_runProperties.dirtyProperties()) {
        writeProperty(writer, entry);
    }

    // 写未知属性（原样）
    for (auto& unknown : m_runProperties.unknowns()) {
        writer.raw(unknown.rawXml);
    }

    writer.endElement(OoxmlNs::w, "rPr");
}
```

### 5.3 增量写出（电子表格优化）

对于 xlsx 的高吞吐场景，全文档写出浪费严重。Worksheet 支持增量写出：

```
Worksheet::writeDirtyCells(XmlWriter& writer)
  → 只序列化 m_dirtyCells 中对应的 <s:c> 元素
  → 外部 Zip Archive 做 in-place 替换对应条目
```

---

## 6. 关系（Relationship）管理

### 6.1 设计

```cpp
// RelationshipMap.h
class RelationshipMap {
public:
    struct RelEntry {
        std::string id;          // "rId4"
        std::string type;        // "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image"
        std::string target;      // "media/image1.png"
        std::string targetMode;  // "Internal" / "External"
    };

    void add(RelEntry entry);
    const RelEntry* findById(const std::string& id) const;
    const RelEntry* findByTarget(const std::string& target) const;

    // 写入 .rels XML 文件
    void writeXml(XmlWriter& writer) const;

private:
    std::vector<RelEntry> m_entries;
};
```

### 6.2 读流程

```
1. 读取 zip 条目 "_rels/.rels" → 根 RelationshipMap
2. 读取 zip 条目 "word/_rels/document.xml.rels" → 文档级 RelationshipMap
3. 解析 document.xml 时遇到 rId4：
   → 查文档级 RelationshipMap → target = "media/image1.png"
   → 创建 DomDrawing { m_relationshipId = "rId4" }
```

### 6.3 写出流程

```
1. 遍历 DOM 树，收集所有 relationshipId 引用
2. 为每个引用生成 RelEntry（id 自动分配或复用原有 ID）
3. 写入 .rels 文件
4. 写入 [Content_Types].xml（每个 target 的类型需要注册）
```

---

## 7. [Content_Types].xml 管理

```cpp
// ContentTypeRegistry.h
class ContentTypeRegistry {
public:
    struct ContentTypeEntry {
        std::string partName;       // "/word/document.xml"
        std::string contentType;    // "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"
    };

    void registerPart(const std::string& partName, const std::string& contentType);

    // 根据扩展名自动推断 Content-Type
    static std::string contentTypeForExtension(const std::string& ext);
    // 根据部件路径推断 Content-Type
    static std::string contentTypeForPart(const std::string& partPath);

    // 写出 [Content_Types].xml
    void writeXml(XmlWriter& writer) const;
};

// 内置 ContentType 常量
namespace ContentTypes {
    // Word
    inline constexpr auto word_main  = "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml";
    inline constexpr auto word_style = "application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml";

    // Excel
    inline constexpr auto sheet_main = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml";
    inline constexpr auto sheet_ws   = "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml";
    inline constexpr auto sheet_str  = "application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml";

    // PPT
    inline constexpr auto ppt_main   = "application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml";
    inline constexpr auto ppt_slide  = "application/vnd.openxmlformats-officedocument.presentationml.slide+xml";

    // Image
    inline constexpr auto png  = "image/png";
    inline constexpr auto jpeg = "image/jpeg";
    inline constexpr auto svg  = "image/svg+xml";
}
```

---

## 8. 扩展的 Schema 兼容性处理

### 8.1 Markup Compatibility (mc:)

```xml
<!-- 示例 -->
<mc:AlternateContent>
  <mc:Choice Requires="w14">
    <w14:glow w14:rad="114300"/>
  </mc:Choice>
  <mc:Fallback>
    <!-- 不支持 w14 的处理器使用这里的内容 -->
  </mc:Fallback>
</mc:AlternateContent>
```

**解析策略：**
1. 遍历 `mc:Choice`，找到第一个 `Requires` 中列出的命名空间我们**支持**的项
2. 用该项的内容创建 DOM 节点
3. 如果所有 `Requires` 都不匹配，使用 `mc:Fallback` 内容
4. 解析完成后，DOM 中不保留 `mc:AlternateContent` 包装（已做出选择）

### 8.2 扩展命名空间忽略规则

```cpp
// 在解析时，以下命名空间的元素不参与 DOM，直接丢弃：
const std::unordered_set<std::string> kIgnorableNamespaces = {
    "http://schemas.openxmlformats.org/markup-compatibility/2006",  // mc:
    // 不添加其他——未知扩展保留为 UnknownElement
};
```

**注意：** 只有 `mc:` 被明确丢弃（因为它已被 "解析" 为选择结果）。所有其他未知命名空间的元素**保留**为 `UnknownElement`，以确保无损读写。

---

## 9. 文档属性（Metadata）映射

```cpp
// DocumentProperties.h
class DocumentProperties {
public:
    // 核心属性 (cp: / dc:)
    std::u16string title;
    std::u16string creator;
    std::u16string lastModifiedBy;
    std::u16string description;
    std::u16string subject;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point modified;

    // 应用属性
    std::u16string application = u"OpenSmartOffice";
    uint32_t totalEditingTime = 0;   // 分钟
    uint32_t wordCount = 0;
    uint32_t pageCount = 0;

    // 写入 docProps/core.xml + docProps/app.xml
    void writeToZip(IZipArchive& archive) const;

    // 从 zip 中读取
    static DocumentProperties readFromZip(IZipArchive& archive);
};
```

---

## 10. 验证与合规

### 10.1 单元测试策略

| 测试类型 | 方法 | 覆盖 |
|---------|------|------|
| 元素解析 | 给定最小 XML 片段 → 解析 → 断言 DOM 节点类型和属性 | 每个注册的元素 |
| 属性往返 | DOM 设置属性 → writeXml → 重新解析 → 属性一致 | PropertyBag 所有已知 Key |
| 文档往返 | 读 docx/xlsx/pptx → 立即写回 → 解压 ZIP → 逐文件 diff | 端到端 |
| 未知元素保留 | 构造含自定义命名空间的 XML → 解析 → 写出 → XML 逐字节一致 | 无损读写 |
| 样式继承 | 多层 style → resolve → 预期 PropertyBag 值 | 样式解析器 |

### 10.2 金牌文件测试集

选取覆盖以下边界 case 的 OOXML 文档作为回归测试：
- 空文档（最少元素）
- 复杂表格（合并单元格、嵌套表格）
- 多节文档（不同页面设置）
- 修订追踪文档
- 多工作表（含跨表引用公式）
- 图表工作表
- 含母版/版式/动画的 PPT
- 含 SmartArt 的 PPT
