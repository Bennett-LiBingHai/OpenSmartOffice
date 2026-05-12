# OpenSmartOffice — 文档对象模型 (DOM) 设计

## 1. 设计原则

| 原则 | 说明 |
|------|------|
| **无损读写** | 解析时保留所有 OOXML 元素，哪怕不认识它。未知元素原样序列化回 XML |
| **类型安全** | 每个 OOXML 元素映射到一个强类型 C++ 类，不做 stringly-typed |
| **最小公共基类** | Word/Excel/PPT 只在确实共享语义时才提取公共基类，避免过度抽象 |
| **不变式由构造函数保证** | 对象构造后即处于合法状态，不用两阶段初始化 |
| **copy-on-write 共享** | 图片、字体等大对象用 shared_ptr 共享，修改时 COW |
| **树形结构** | DOM 是严格的树，父子关系明确，无循环引用 |

---

## 2. 公共 DOM 基类 (`core/dom/common/`)

### 2.1 节点类型枚举

```cpp
// DomNodeType.h
enum class DomNodeType : uint16_t {
    // ---- Common (0x0000~0x00FF) ----
    Document       = 0x0000,
    Text           = 0x0001,
    Drawing        = 0x0002,   // 图片/形状 (DrawingML)
    Hyperlink      = 0x0003,
    Comment        = 0x0004,
    UnknownElement = 0x00FF,   // 未识别的 OOXML 元素，保留原始 XML

    // ---- Word (0x0100~0x01FF) ----
    WordDocument   = 0x0100,
    Body           = 0x0101,
    Paragraph      = 0x0102,
    Run            = 0x0103,
    Table          = 0x0104,
    TableRow       = 0x0105,
    TableCell      = 0x0106,
    Section        = 0x0107,
    HeaderFooter   = 0x0108,
    FieldCode      = 0x0109,
    Bookmark       = 0x010A,

    // ---- Spreadsheet (0x0200~0x02FF) ----
    Workbook       = 0x0200,
    Worksheet      = 0x0201,
    Cell           = 0x0202,
    CellRange      = 0x0203,
    Row            = 0x0204,
    Column         = 0x0205,
    Formula        = 0x0206,
    PivotTable     = 0x0207,
    Chart          = 0x0208,

    // ---- Presentation (0x0300~0x03FF) ----
    Presentation   = 0x0300,
    Slide          = 0x0301,
    SlideMaster    = 0x0302,
    SlideLayout    = 0x0303,
    Shape          = 0x0304,
    TextFrame      = 0x0305,
    Animation      = 0x0306,
    Transition     = 0x0307,
    Note           = 0x0308,
};
```

### 2.2 基础节点类

```cpp
// DomNode.h
class DomNode {
public:
    // ---- 树操作 ----
    DomNode* parent() const;
    size_t childCount() const;
    DomNode* childAt(size_t index) const;
    DomNode* firstChild() const;
    DomNode* lastChild() const;
    DomNode* nextSibling() const;
    DomNode* prevSibling() const;

    // 插入/删除子节点（转移所有权）
    Result<void> appendChild(std::unique_ptr<DomNode> child);
    Result<void> insertBefore(std::unique_ptr<DomNode> child, DomNode* ref);
    std::unique_ptr<DomNode> removeChild(DomNode* child);
    void removeAllChildren();

    // ---- 类型 ----
    DomNodeType nodeType() const;

    // ---- 序列化标记 ----
    // 标记此节点（及其子树）自上次保存后是否被修改
    bool isDirty() const;
    void setDirty(bool dirty);
    // 传播 dirty 标记到根节点
    void markDirty();

    // ---- OOXML 保留字段 ----
    // 如果此节点是 UnknownElement，保留原始 XML 命名空间和元素名
    // 所有节点都保留解析时附带的未知属性和命名空间声明
    const std::vector<UnknownAttribute>& unknownAttributes() const;
    void addUnknownAttribute(UnknownAttribute attr);

protected:
    DomNode(DomNodeType type);
    virtual ~DomNode();

private:
    DomNodeType m_type;
    DomNode* m_parent = nullptr;
    std::vector<std::unique_ptr<DomNode>> m_children;
    bool m_dirty = false;
    std::vector<UnknownAttribute> m_unknownAttributes;

    // 禁止拷贝
    DomNode(const DomNode&) = delete;
    DomNode& operator=(const DomNode&) = delete;
};

struct UnknownAttribute {
    std::string namespaceUri;
    std::string localName;
    std::string prefix;
    std::string value;
};
```

### 2.3 文本节点

```cpp
// DomText.h
class DomText : public DomNode {
public:
    explicit DomText(std::u16string text);
    const std::u16string& text() const;
    void setText(std::u16string text);   // 自动 markDirty()

private:
    std::u16string m_text;
};
```

### 2.4 编码约定

OOXML 文件中的 XML 内容使用 UTF-8 编码。DOM 内部文本统一使用 `std::u16string`（UTF-16）存储，原因：

1. Qt 内部使用 UTF-16（`QString` 内部为 UTF-16），GUI 渲染无需编码转换
2. OOXML 中 `s:c v="..."` 的字符串值可能包含代理对（如 emoji），UTF-16 原生支持
3. 公式引擎 ixion 内部使用 UTF-16 字符串

**编码转换位置：** SAX → DOM 桥接层（`OoxmlSaxHandler::characters()`）。`IOoxmlReader` 的回调使用 `std::string`（UTF-8），`OoxmlSaxHandler` 在构造 `DomText` 之前执行 UTF-8 → UTF-16 转换（使用 `QString::fromUtf8()` 或 `std::codecvt`）。

### 2.5 通用图片/形状节点（来自 DrawingML）

```cpp
// Drawing.h
class DomDrawing : public DomNode {
public:
    enum class DrawingType { Image, Shape, Chart, SmartArt };

    DrawingType drawingType() const;
    const std::string& relationshipId() const;   // rId → 图片文件在 zip 中的路径
    Rect bounds() const;                         // 在页面/单元格中的位置
    void setBounds(Rect bounds);

private:
    DrawingType m_drawingType;
    std::string m_relationshipId;
    Rect m_bounds;
};
```

---

## 3. 属性系统 (`PropertyBag`)

### 3.1 设计目标

OOXML 元素有大量属性：字体、颜色、边框、对齐、缩进、间距……这些属性：
- 大量重复出现（每个 Run 都可能有字体属性）
- 有继承关系（段落样式 → 字符样式 → 直接格式）
- 有默认值（不写 = 使用默认）

直接用 getter/setter 会把类撑爆（Word 的 Run 有 ~50 个属性），所以引入 `PropertyBag` 作为**按需存储的属性字典**。

### 3.2 核心设计

```cpp
// PropertyBag.h
class PropertyBag {
public:
    // ---- 类型安全的属性访问 ----
    template <typename T>
    Result<std::optional<T>> get(PropertyKey key) const;

    template <typename T>
    void set(PropertyKey key, T value);

    template <typename T>
    void clear(PropertyKey key);

    bool has(PropertyKey key) const;

    // ---- 合并（用于样式继承） ----
    // 从低优先级样式继承：仅填入 this 中缺失的属性（this 已有属性保留，即直接格式优先）
    // 用于：直接格式 → 合并样式继承链 → 得到最终格式
    void inheritFrom(const PropertyBag& lowerPriority);

    // 从高优先级样式覆盖：用 other 的属性覆盖 this 的同名属性
    // 用于：重新计算完整样式时，按优先级从低到高逐步覆盖
    void overrideWith(const PropertyBag& higherPriority);

    // ---- 与默认值合并 ----
    // 对每个未设置的 key，填入默认值（来自 OOXML 规范）
    void applyDefaults(const PropertyBag& defaults);

    // ---- 序列化 ----
    // 仅序列化非默认值 / 非继承值的属性（dirty properties）
    std::vector<PropertyEntry> dirtyProperties() const;

    // ---- 未知属性保留 ----
    const std::vector<UnknownProperty>& unknowns() const;

private:
    std::unordered_map<PropertyKey, PropertyValue> m_properties;
    std::vector<UnknownProperty> m_unknowns;
};

// PropertyKey: {namespacePrefix, localName} 的组合
// 对于 OOXML 中一个元素包含多个子属性的情况（如 w:spacing 有 line/before/after），
// 使用点分表示法：{"w", "spacing.line"}, {"w", "spacing.before"} 等
// 例如: PropertyKey("w", "b") 代表 w:b (加粗)
//       PropertyKey("w", "spacing.line") 代表 w:spacing 的 w:line 属性 (行间距)
struct PropertyKey {
    std::string namespacePrefix;
    std::string localName;      // 支持点分表示法: "spacing.line"
    // hash + operator== ...
};

struct UnknownProperty {
    PropertyKey key;
    std::string rawXml;  // 原始 XML 片段，用于无损回写
};
```

### 3.3 PropertyKey 常量定义

```cpp
// PropertyKeys.h — 集中定义所有已知属性 Key
namespace PropertyKeys {
    // — 字体 —
    inline const PropertyKey Font_Bold      {"w", "b"};
    inline const PropertyKey Font_Italic    {"w", "i"};
    inline const PropertyKey Font_Size      {"w", "sz"};     // 半磅值
    inline const PropertyKey Font_Name      {"w", "rFonts"};
    inline const PropertyKey Font_Color     {"w", "color"};
    inline const PropertyKey Font_Underline {"w", "u"};

    // — 段落 —
    // OOXML 中 w:spacing 是一个元素，通过 w:line / w:before / w:after 属性区分
    // PropertyKey 使用 "元素.属性" 的点分表示法区分同一元素的不同属性
    inline const PropertyKey Para_Alignment      {"w", "jc"};
    inline const PropertyKey Para_LineSpacing    {"w", "spacing.line"};
    inline const PropertyKey Para_SpaceBefore    {"w", "spacing.before"};
    inline const PropertyKey Para_SpaceAfter     {"w", "spacing.after"};
    inline const PropertyKey Para_IndentLeft     {"w", "ind.left"};
    inline const PropertyKey Para_IndentRight    {"w", "ind.right"};
    inline const PropertyKey Para_IndentFirstLine {"w", "ind.firstLine"};
    inline const PropertyKey Para_IndentHanging  {"w", "ind.hanging"};

    // — 单元格 —
    inline const PropertyKey Cell_NumberFormat  {"s", "numFmtId"};
    inline const PropertyKey Cell_FillColor     {"s", "fgColor"};
    inline const PropertyKey Cell_BorderLeft    {"s", "left"};
    inline const PropertyKey Cell_Alignment     {"s", "alignment"};

    // — 形状 —
    inline const PropertyKey Shape_Fill     {"a", "solidFill"};
    inline const PropertyKey Shape_Outline  {"a", "ln"};
    inline const PropertyKey Shape_Shadow   {"a", "effectLst"};

    // ... 完整列表见 OOXML 映射文档
}
```

### 3.4 属性值类型映射

```cpp
// 边框样式 — 对应 OOXML w:ln / a:ln 的边框属性
struct BorderStyle {
    enum class Style { None, Single, Double, Dotted, Dashed, DashDot, DashDotDot, Thick, Thin };
    Style style = Style::None;
    double width = 0;          // EMU
    Color color;
    double spacing = 0;        // 阴影偏移（部分边框类型使用）
};

// 填充图案 — 对应 OOXML a:gradFill / a:pattFill / a:solidFill
struct FillPattern {
    enum class Type { None, Solid, Gradient, Pattern, Picture };
    Type type = Type::None;
    Color foregroundColor;
    Color backgroundColor;
    double angle = 0;          // 渐变角度（仅 Gradient 类型）
};

using PropertyValue = std::variant<
    bool,
    int32_t,
    uint32_t,
    double,
    std::string,       // 枚举值 / 字符串
    std::u16string,    // 用户可见文本
    Color,
    Rect,
    BorderStyle,       // 边框样式
    FillPattern        // 填充图案
>;
```

### 3.5 属性系统不适用的场景

以下属性不通过 PropertyBag 存储，直接作为节点类的成员字段：

| 属性类型 | 存储方式 | 原因 |
|---------|---------|------|
| 关系引用（rId） | `DomDrawing::m_relationshipId` | 关系 ID 是跨部件引用，不属于元素自身属性，需要在写出时参与 Relationship 重建 |
| 单元格样式索引 | `Cell::m_styleIndex` | Excel 使用索引引用样式表，不是内联属性 |
| 单元格引用 | `Cell::m_ref`（CellRef） | A1 引用是单元格的身份标识，不是可继承属性 |
| 形状 ID | `Shape::m_shapeId` | 幻灯片中形状的唯一标识，用于动画目标引用 |
| 母版/版式 ID | `SlideMaster::m_masterId` / `SlideLayout::m_layoutId` | 关系引用标识 |

**规则：** 如果一个属性需要跨节点查找或参与关系重建，它应作为节点字段而非 PropertyBag 条目。

---

## 4. Word 文档 DOM (`core/dom/word/`)

### 4.1 文档树结构

```
WordDocument
├── Body
│   ├── Section (页面设置: 纸张大小、边距、分栏)
│   │   ├── Paragraph
│   │   │   ├── Run (连续相同格式的文本段)
│   │   │   │   ├── DomText "Hello "
│   │   │   │   └── DomDrawing (内嵌图片)
│   │   │   ├── Run
│   │   │   │   └── DomText "World"
│   │   │   └── Hyperlink
│   │   │       └── Run → DomText "click here"
│   │   ├── Table
│   │   │   ├── TableRow
│   │   │   │   ├── TableCell → Paragraph → Run → DomText
│   │   │   │   └── TableCell → Paragraph → Run → DomText
│   │   │   └── TableRow ...
│   │   └── Paragraph ...
│   └── Section ...
├── HeaderFooter (odd/even/first, 每种一个)
│   ├── Paragraph ...
│   └── Table ...
└── Footnotes / Endnotes (可选)
```

### 4.2 核心类

```cpp
// WordDocument.h
class WordDocument : public DomNode {
public:
    // 样式表（文档级）
    const StyleSheet& styleSheet() const;
    StyleSheet& mutableStyleSheet();

    // 页眉页脚
    HeaderFooter* header(HeaderFooterType type);
    HeaderFooter* footer(HeaderFooterType type);

    // 编号定义
    const NumberingDefinitions& numbering() const;

    // Body 便捷访问
    Body* body();
};

enum class HeaderFooterType { Default, First, Even };

// StyleSheet.h
class StyleSheet {
public:
    // 段落样式 (w:style w:type="paragraph")
    const PropertyBag* paragraphStyle(const std::string& styleId) const;
    // 字符样式
    const PropertyBag* characterStyle(const std::string& styleId) const;
    // 表格样式
    const PropertyBag* tableStyle(const std::string& styleId) const;
    // 默认样式
    const PropertyBag& defaultParagraphProps() const;
    const PropertyBag& defaultRunProps() const;

    // 解析后的完整样式（已合并继承链）
    PropertyBag resolveParagraphStyle(const std::string& styleId) const;
    PropertyBag resolveCharacterStyle(const std::string& styleId) const;
};

// Paragraph.h
class Paragraph : public DomNode {
public:
    PropertyBag& paragraphProperties();
    const PropertyBag& paragraphProperties() const;
};

// Run.h
class Run : public DomNode {
public:
    PropertyBag& runProperties();
    const PropertyBag& runProperties() const;
};

// Table.h / TableRow.h / TableCell.h
class TableCell : public DomNode {
public:
    // 单元格合并信息
    std::optional<int> gridSpan() const;
    std::optional<int> vMerge() const;   // "restart" / "continue"
    PropertyBag& cellProperties();
};

// Section.h
class Section : public DomNode {
public:
    PropertyBag& sectionProperties();
    // 页面尺寸、边距、分栏设置 通过 PropertyBag 访问
};
```

### 4.3 样式解析顺序

```
Run 的实际格式计算（从低优先级到高优先级逐步覆盖）：
  1. defaultRunProps (StyleSheet) — 最低优先级
  2. → overrideWith(characterStyle) — 字符样式覆盖默认值
  3. → overrideWith(paragraphStyle) — 段落样式中的字符属性覆盖
  4. → overrideWith(runProperties) — Run 直接属性覆盖
  5. → overrideWith(直接格式元素) — 如 w:b/w:i 等独立元素，最高优先级

  等价写法（从最终结果反向填充缺失值）：
  result = directFormat;
  result.inheritFrom(runProperties);        // 填入缺失的 run 属性
  result.inheritFrom(characterStyle);        // 填入缺失的字符样式
  result.inheritFrom(defaultRunProps);       // 填入默认值
```

---

## 5. 电子表格 DOM (`core/dom/sheet/`)

### 5.1 文档树结构

```
Workbook
├── SharedStrings (共享字符串表)
│   ├── DomText "Name"
│   ├── DomText "Age"
│   └── DomText "City"
├── Stylesheet (单元格样式)
│   ├── NumberFormat[]
│   ├── Fill[]
│   ├── Font[]
│   ├── Border[]
│   └── CellFormat[] (索引组合以上各项)
├── Worksheet (Sheet1)
│   ├── Column[] (列宽、隐藏)
│   ├── Row[]
│   │   ├── Cell { ref: A1, type: String, value: sstIndex=0 }
│   │   ├── Cell { ref: B1, type: Number, value: 42.0 }
│   │   └── Cell { ref: C1, type: Formula, formula: "=A1+B1" }
│   │       └── Formula
│   ├── MergeCells
│   ├── AutoFilter
│   └── Drawing[] (图表、图片)
├── Worksheet (Sheet2) ...
└── PivotCache / PivotTable
```

### 5.2 核心类

```cpp
// Workbook.h
class Workbook : public DomNode {
public:
    // 共享字符串表
    SharedStrings& sharedStrings();
    // 样式表
    CellStylesheet& stylesheet();
    // 工作表集合
    size_t sheetCount() const;
    Worksheet* sheetAt(size_t index);
    Worksheet* sheetByName(const std::u16string& name);
    Result<void> addSheet(std::unique_ptr<Worksheet> sheet);
    // 命名范围
    const NamedRangeMap& namedRanges() const;
};

// Worksheet.h
class Worksheet : public DomNode {
public:
    const std::u16string& name() const;
    uint32_t sheetId() const;

    Cell* cellAt(CellRef ref);
    Cell* cellAt(uint32_t row, uint32_t col);
    const Cell* cellAt(CellRef ref) const;

    Result<Cell*> getOrCreateCell(CellRef ref);

    // 遍历所有非空单元格
    void forEachCell(std::function<void(Cell&)> callback);

    Column* columnAt(uint32_t col);
    Row* rowAt(uint32_t row);

    // 合并单元格
    Result<void> mergeCells(CellRange range);
    void unmergeCells(CellRange range);
    const std::vector<CellRange>& mergedCells() const;

    uint32_t maxRow() const;  // 实际使用的最大行号
    uint32_t maxCol() const;  // 实际使用的最大列号
};

// Cell.h
class Cell : public DomNode {
public:
    CellRef ref() const;  // 例如 A1, Z100

    CellType type() const;
    enum class CellType { Empty, Boolean, Number, String, Error, Formula };

    // 值（已缓存的计算结果）
    CellValue value() const;
    void setValue(CellValue value);

    // 公式（仅当 type == Formula 时有效）
    const std::u16string* formulaText() const;
    void setFormula(std::u16string formula);

    // 样式索引（指向 CellStylesheet）
    uint32_t styleIndex() const;
    void setStyleIndex(uint32_t index);

    // 直接属性访问（便捷接口，最终映射到 styleIndex 对应的 CellFormat）
    PropertyBag& directProperties();
};

// CellValue.h
using CellValue = std::variant<
    std::monostate,      // Empty
    bool,                // Boolean
    double,              // Number
    std::u16string,      // String
    ErrorValue           // Error
>;

enum class ErrorValue {
    Null, Ref, Value, Div0, NA, Name, Num, Spill, Calc
};

// CellRef.h
struct CellRef {
    uint32_t col;  // 0-based (A=0)
    uint32_t row;  // 0-based (1=0)
    // string ↔ index 互相转换
    static CellRef fromString(const std::string& ref);  // "A1" → {0,0}
    std::string toString() const;                        // {0,0} → "A1"
};

// CellRange.h
struct CellRange {
    CellRef topLeft;
    CellRef bottomRight;
};

// CellStylesheet.h
class CellStylesheet {
public:
    struct CellFormat {
        uint32_t numFmtId;
        uint32_t fontId;
        uint32_t fillId;
        uint32_t borderId;
    };

    const CellFormat& cellFormat(uint32_t index) const;
    const PropertyBag& font(uint32_t index) const;
    const PropertyBag& fill(uint32_t index) const;
    const PropertyBag& border(uint32_t index) const;
    uint32_t numberFormatId(uint32_t index) const;
};
```

### 5.3 稀疏存储策略

Excel 工作表最多 1,048,576 行 × 16,384 列，不能为每个单元格分配对象。

```cpp
// Worksheet 内部使用稀疏存储
class Worksheet : public DomNode {
private:
    // 分层稀疏索引：RowIndex → (ColIndex → Cell)
    // 只有非空单元格占用内存
    std::unordered_map<uint32_t, std::unique_ptr<Row>> m_rows;
    uint32_t m_maxRow = 0;
    uint32_t m_maxCol = 0;
};

// Row 内部
class Row : public DomNode {
private:
    std::unordered_map<uint32_t, std::unique_ptr<Cell>> m_cells;
};
```

空单元格不分配对象，`cellAt()` 返回 `nullptr`。

---

## 6. 演示文稿 DOM (`core/dom/slide/`)

### 6.1 文档树结构

```
Presentation
├── SlideMaster[] (母版，定义背景/占位符布局)
│   ├── SlideLayout[] (版式)
│   │   ├── Shape (标题占位符)
│   │   ├── Shape (内容占位符)
│   │   └── Shape (页脚占位符)
│   └── Shape (母版背景)
├── Slide[]
│   ├── Shape (文本框 → TextFrame → Paragraph → Run)
│   ├── Shape (图片)
│   ├── Shape (表格)
│   ├── Shape (图表)
│   ├── Transition (切换动画)
│   └── Note (演讲者备注)
├── Slide 2 ...
└── Slide 3 ...
```

### 6.2 核心类

```cpp
// Presentation.h
class Presentation : public DomNode {
public:
    const Size& slideSize() const;
    void setSlideSize(Size size);

    // 母版
    size_t masterCount() const;
    SlideMaster* masterAt(size_t index);
    void addMaster(std::unique_ptr<SlideMaster> master);

    // 幻灯片
    size_t slideCount() const;
    Slide* slideAt(size_t index);
    Result<void> addSlide(std::unique_ptr<Slide> slide, size_t index);
    void removeSlide(size_t index);
    Result<void> moveSlide(size_t from, size_t to);

    // 默认文本样式
    PropertyBag& defaultTextStyle();
};

// SlideMaster.h
class SlideMaster : public DomNode {
public:
    const std::string& masterId() const;
    // 版式
    size_t layoutCount() const;
    SlideLayout* layoutAt(size_t index);
    // 母版级形状（背景等）
    const std::vector<std::unique_ptr<Shape>>& shapes() const;
};

// SlideLayout.h
class SlideLayout : public DomNode {
public:
    const std::string& layoutId() const;
    // 占位符定义
    const std::vector<Placeholder>& placeholders() const;
};

struct Placeholder {
    enum class Type { Title, Body, Image, Table, Chart, Footer, Date, SlideNumber };
    Type type;
    Rect bounds;
    uint32_t idx;  // OOXML placeholder index
};

// Slide.h
class Slide : public DomNode {
public:
    // 所属版式引用
    const std::string& layoutId() const;

    // 形状集合（文本框、图片、表格、图表等）
    size_t shapeCount() const;
    Shape* shapeAt(size_t index);
    void addShape(std::unique_ptr<Shape> shape);
    void removeShape(size_t index);
    Result<void> reorderShape(size_t from, size_t to);  // z-order

    // 切换效果
    const Transition* transition() const;
    void setTransition(Transition trans);

    // 演讲者备注
    const std::u16string& notes() const;
    void setNotes(std::u16string notes);

    // 是否隐藏
    bool isHidden() const;
    void setHidden(bool hidden);
};

// Shape.h
class Shape : public DomNode {
public:
    enum class ShapeType { TextBox, Image, Table, Chart, SmartArt, Group, Connector };

    ShapeType shapeType() const;

    const std::string& shapeId() const;
    const Rect& bounds() const;
    void setBounds(Rect bounds);
    float rotation() const;   // 旋转角度 (degrees)
    void setRotation(float degrees);

    // 填充和轮廓
    PropertyBag& shapeProperties();

    // 文本框内容（仅 TextBox 有效）
    TextFrame* textFrame();

    // z-order
    uint32_t zOrder() const;
    void setZOrder(uint32_t order);
};

// TextFrame.h
class TextFrame : public DomNode {
public:
    // 自动调整行为
    enum class AutoSize { None, ShrinkToFit, ResizeToFit };
    AutoSize autoSize() const;

    // 内部边距
    const Rect& insets() const;

    // 段落集合（与 Word 共用 Paragraph / Run）
    // TextFrame 内部是一组 Paragraph 节点
};

// Transition.h
struct Transition {
    enum class Type {
        None, Fade, Push, Wipe, Split, Reveal, Cover, Clock, Zoom, Random
    };
    Type type = Type::None;
    float duration = 0.5f;     // 秒
    bool onMouseClick = true;
    float autoAdvanceAfter = 0; // 秒, 0 表示不自动切换
};

// Animation.h — 幻灯片内元素动画
class Animation : public DomNode {
    enum class Effect { Appear, Fade, FlyIn, Zoom, Spin, Bounce };
    enum class Trigger { OnClick, WithPrevious, AfterPrevious };

    Effect effect;
    Trigger trigger;
    float duration;     // 秒
    float delay;        // 秒
    std::string targetShapeId;  // 动画应用到的形状
};
```

---

## 7. 样式系统设计

### 7.1 三种文档的样式差异

| 文档类型 | 样式粒度 | 继承模型 | 共享范围 |
|---------|---------|---------|---------|
| Word | 段落样式、字符样式、表格样式 | 多级继承 (basedOn)，最终合并直接格式 | 文档内 |
| Excel | 单元格格式索引 (Font×Fill×Border×NumFmt) | 无继承，按索引引用 | 工作簿内 |
| PPT | 母版 → 版式 → 幻灯片，三级覆盖 | 母版级继承，幻灯片可覆盖 | 演示文稿内 |

### 7.2 统一样式表示

虽然三种文档内部样式机制不同，但最终都转化为 `PropertyBag`：

```
Word:   StyleSheet::resolve("Heading1") → PropertyBag
Excel:  stylesheet.cellFormat(fmtIdx) + font(fontId) + fill(fillId) → PropertyBag
PPT:    masterDefaults → layoutDefaults → slideProps → PropertyBag
```

渲染层只需要一个 `PropertyBag`，不关心它是怎么解出来的。

---

## 8. 无损读写机制

### 8.1 未知元素保留

```
OOXML 解析时：
  已知元素 → 创建对应的 C++ DOM 类实例
  未知元素 → 创建 UnknownElement 节点，保留：
            - 完整 XML outerXml (序列化时原样写出)
            - 所属 namespace
            - 元素名
            - 所有属性
            - 所有子节点递归保留
```

### 8.2 未知属性保留

```
已知属性 → 存入 PropertyBag 的对应 key
未知属性 → 存入 PropertyBag::m_unknowns，保留原始 XML 字符串
          序列化时原样写出，不解析、不修改
```

### 8.3 DOM ↔ XML 映射接口

```cpp
// core/dom/common/DomNode.h

// 每个 DOM 节点提供序列化钩子
class DomNode {
public:
    // 将自身序列化为 XML（子类实现）
    virtual void writeXml(XmlWriter& writer) const;

    // 从 SAX 事件构建（子类实现静态工厂）
    // 由 OOXML Reader 在解析时调用
};
```

### 8.4 验证方法

```
测试方法：
1. 读取 docx → 立即写回 docx_new
2. 将 docx_new 重命名为 zip，解压
3. 逐个对比原始 docx 解压后的 XML 文件：
   - 已知元素：确保语义等价的 XML（不要求逐字节相同）
   - 未知元素：确保逐字节相同
```

---

## 9. 跨文档引用

OOXML 中一个元素可能引用另一个文件中的资源，通过 Relationship 机制。

```
docx 内部结构：
  /word/document.xml     ← 主文档，通过 rId 引用其他部件
  /word/styles.xml       ← 样式表
  /word/media/image1.png ← 图片
  /word/header1.xml      ← 页眉
  /word/_rels/document.xml.rels  ← 关系映射 (rId → 目标路径)

xlsx 内部结构：
  /xl/workbook.xml       ← 工作簿，定义工作表列表
  /xl/worksheets/sheet1.xml
  /xl/sharedStrings.xml  ← 共享字符串
  /xl/styles.xml         ← 样式表
  /xl/_rels/workbook.xml.rels
```

### 9.1 内部关系管理

```cpp
// 在读取时，Relationship 信息注入 DOM
// 每个节点如果需要引用关系，存储 relationshipId
// 读取引擎负责在 serialize 时重新建立 rId → 路径映射

class DomDrawing : public DomNode {
    // 图片内容不存储在 DOM 中，只存关系 ID
    // 原始图片字节存在 ZipArchive 中
    std::string m_relationshipId;   // "rId4"
    std::string m_imagePath;        // "media/image1.png" (缓存的解析结果)
};
```

### 9.2 图片数据的获取

```cpp
// 图片字节不常驻 DOM，按需从 Zip 中提取
// 这避免了加载大量图片时内存爆炸

Result<std::vector<uint8_t>> DocumentFacade::extractImage(
    const DomDrawing& drawing) {
    auto relId = drawing.relationshipId();
    auto path = m_zipArchive->resolveRelationship(relId);
    return m_zipArchive->readEntry(path);
}
```

---

## 10. DOM 变更通知

### 10.1 Dirty 标记传播

```
任一节点修改 → markDirty() 
  → 向上传播到根 Document 
  → 根 Document 设置 m_needsSave = true

保存时：
  if (!document->isDirty()) return; // 跳过
  writeXml(...);
  document->clearDirty(); // 清除整棵树的 dirty 标记
```

### 10.2 增量更新（电子表格特需）

Excel 的单元格修改不能每次触发全文档保存。采用脏单元格追踪：

```cpp
class Worksheet : public DomNode {
private:
    std::unordered_set<CellRef> m_dirtyCells;
public:
    void markCellDirty(CellRef ref) {
        m_dirtyCells.insert(ref);
        markDirty();  // 同时传播文档级 dirty
    }
    const std::unordered_set<CellRef>& dirtyCells() const { return m_dirtyCells; }
    void clearDirtyCells() { m_dirtyCells.clear(); }
};
```

对于高吞吐场景（200 docx/s），增量序列化只写脏单元格对应的 XML 片段，避免全量 XML 序列化。
