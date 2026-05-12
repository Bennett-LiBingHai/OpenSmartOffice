#include <gtest/gtest.h>
#include "oso/dom/word/WordDocument.h"
#include "oso/dom/common/DomNode.h"
#include "oso/ooxml/write/Libxml2Writer.h"
#include "oso/ooxml/read/Libxml2Reader.h"
#include "oso/io/IStream.h"

using namespace oso;

// ================================================================
// WordDocument
// ================================================================

TEST(WordDocument, Construction) {
    WordDocument doc;
    EXPECT_EQ(doc.documentType(), "word");
    EXPECT_EQ(doc.localName(), "document");
    EXPECT_EQ(doc.nodeType(), DomNodeType::Document);
}

TEST(WordDocument, NamespaceUriIsCorrect) {
    WordDocument doc;
    EXPECT_FALSE(doc.namespaceUri().empty());
    EXPECT_NE(doc.namespaceUri().find("wordprocessingml"), std::string::npos);
}

TEST(WordDocument, BodyReturnsNullWhenEmpty) {
    WordDocument doc;
    EXPECT_EQ(doc.body(), nullptr);
}

TEST(WordDocument, SetBody) {
    WordDocument doc;
    auto body = std::make_unique<Body>();
    auto* bodyRaw = body.get();
    doc.setBody(std::move(body));

    EXPECT_EQ(doc.body(), bodyRaw);
    EXPECT_GE(doc.childCount(), 1u);
}

TEST(WordDocument, SetBodyReplacesExisting) {
    WordDocument doc;
    auto body1 = std::make_unique<Body>();
    auto* b1 = body1.get();
    doc.setBody(std::move(body1));
    EXPECT_EQ(doc.body(), b1);

    auto body2 = std::make_unique<Body>();
    auto* b2 = body2.get();
    doc.setBody(std::move(body2));
    EXPECT_EQ(doc.body(), b2);
}

// ================================================================
// Body
// ================================================================

TEST(Body, Construction) {
    Body body;
    EXPECT_EQ(body.localName(), "body");
    EXPECT_EQ(body.nodeType(), DomNodeType::Element);
    EXPECT_FALSE(body.namespaceUri().empty());
}

TEST(Body, AddAndListParagraphs) {
    Body body;
    auto p1 = std::make_unique<Paragraph>();
    auto p2 = std::make_unique<Paragraph>();
    auto* p1Raw = p1.get();
    auto* p2Raw = p2.get();

    body.addParagraph(std::move(p1));
    body.addParagraph(std::move(p2));

    auto paras = body.paragraphs();
    ASSERT_EQ(paras.size(), 2u);
    EXPECT_EQ(paras[0], p1Raw);
    EXPECT_EQ(paras[1], p2Raw);
}

TEST(Body, LastParagraph) {
    Body body;
    EXPECT_EQ(body.lastParagraph(), nullptr);

    auto p1 = std::make_unique<Paragraph>();
    auto* p1Raw = p1.get();
    body.addParagraph(std::move(p1));
    EXPECT_EQ(body.lastParagraph(), p1Raw);

    auto p2 = std::make_unique<Paragraph>();
    auto* p2Raw = p2.get();
    body.addParagraph(std::move(p2));
    EXPECT_EQ(body.lastParagraph(), p2Raw);
}

TEST(Body, ParagraphsFiltersNonParagraphs) {
    Body body;
    body.appendChild(std::make_unique<DomElement>("nonPara"));
    body.addParagraph(std::make_unique<Paragraph>());

    // paragraphs() 只返回 localName == "p" 的元素
    auto paras = body.paragraphs();
    EXPECT_EQ(paras.size(), 1u);
}

// ================================================================
// Paragraph
// ================================================================

TEST(Paragraph, Construction) {
    Paragraph para;
    EXPECT_EQ(para.localName(), "p");
    EXPECT_EQ(para.nodeType(), DomNodeType::Element);
}

TEST(Paragraph, AddAndListRuns) {
    Paragraph para;
    auto r1 = std::make_unique<oso::Run>();
    auto r2 = std::make_unique<oso::Run>();
    auto* r1Raw = r1.get();
    auto* r2Raw = r2.get();

    para.addRun(std::move(r1));
    para.addRun(std::move(r2));

    auto runs = para.runs();
    ASSERT_EQ(runs.size(), 2u);
    EXPECT_EQ(runs[0], r1Raw);
    EXPECT_EQ(runs[1], r2Raw);
}

TEST(Paragraph, LastRun) {
    Paragraph para;
    EXPECT_EQ(para.lastRun(), nullptr);

    auto r1 = std::make_unique<oso::Run>();
    auto* r1Raw = r1.get();
    para.addRun(std::move(r1));
    EXPECT_EQ(para.lastRun(), r1Raw);
}

TEST(Paragraph, RunsFiltersNonRuns) {
    Paragraph para;
    para.appendChild(std::make_unique<DomElement>("nonRun"));
    para.addRun(std::make_unique<oso::Run>());

    auto runs = para.runs();
    EXPECT_EQ(runs.size(), 1u);
}

// ================================================================
// Run
// ================================================================

TEST(Run, Construction) {
    oso::Run run;
    EXPECT_EQ(run.localName(), "r");
    EXPECT_EQ(run.nodeType(), DomNodeType::Element);
}

TEST(Run, AddTextAndTexts) {
    oso::Run run;
    auto t1 = std::make_unique<Text>();
    auto t2 = std::make_unique<Text>();
    t1->setContent(std::make_unique<DomText>("First"));
    t2->setContent(std::make_unique<DomText>("Second"));
    auto* t1Raw = t1.get();
    auto* t2Raw = t2.get();

    run.addText(std::move(t1));
    run.addText(std::move(t2));

    auto texts = run.texts();
    ASSERT_EQ(texts.size(), 2u);
    EXPECT_EQ(texts[0], t1Raw);
    EXPECT_EQ(texts[1], t2Raw);
}

TEST(Run, TextContent) {
    oso::Run run;
    EXPECT_EQ(run.textContent(), "");

    auto t1 = std::make_unique<Text>();
    t1->setContent(std::make_unique<DomText>("Hello "));
    run.addText(std::move(t1));

    auto t2 = std::make_unique<Text>();
    t2->setContent(std::make_unique<DomText>("World"));
    run.addText(std::move(t2));

    EXPECT_EQ(run.textContent(), "Hello World");
}

TEST(Run, TextContentWithMixedChildren) {
    oso::Run run;
    run.appendChild(std::make_unique<DomElement>("rPr"));

    auto t = std::make_unique<Text>();
    t->setContent(std::make_unique<DomText>("Main text"));
    run.addText(std::move(t));

    // textContent 只拼接 Text 类型的子节点，跳过 rPr
    EXPECT_EQ(run.textContent(), "Main text");
}

// ================================================================
// Text
// ================================================================

TEST(Text, Construction) {
    Text text;
    EXPECT_EQ(text.localName(), "t");
    EXPECT_EQ(text.nodeType(), DomNodeType::Element);

    // 构造时自动创建一个空的 DomText 子节点
    EXPECT_NE(text.content(), nullptr);
    EXPECT_EQ(text.content()->text(), "");
    EXPECT_EQ(text.content()->nodeType(), DomNodeType::Text);
}

TEST(Text, Content) {
    Text text;
    ASSERT_NE(text.content(), nullptr);
    EXPECT_EQ(text.content()->text(), "");
}

TEST(Text, SetContent) {
    Text text;
    auto domText = std::make_unique<DomText>("Hello OpenSmartOffice");
    text.setContent(std::move(domText));

    ASSERT_NE(text.content(), nullptr);
    EXPECT_EQ(text.content()->text(), "Hello OpenSmartOffice");
}

TEST(Text, SetContentReplacesExisting) {
    Text text;
    text.setContent(std::make_unique<DomText>("first"));
    text.setContent(std::make_unique<DomText>("second"));

    ASSERT_NE(text.content(), nullptr);
    EXPECT_EQ(text.content()->text(), "second");
}

TEST(Text, SetContentLeadingWhitespaceSetsXmlSpace) {
    Text text;
    text.setContent(std::make_unique<DomText>("  leading"));

    EXPECT_TRUE(text.hasAttribute("xml:space"));
    EXPECT_EQ(text.getAttribute("xml:space"), "preserve");
}

TEST(Text, SetContentTrailingWhitespaceSetsXmlSpace) {
    Text text;
    text.setContent(std::make_unique<DomText>("trailing  "));

    EXPECT_TRUE(text.hasAttribute("xml:space"));
}

TEST(Text, SetContentBothEndsWhitespaceSetsXmlSpace) {
    Text text;
    text.setContent(std::make_unique<DomText>("  both  "));

    EXPECT_TRUE(text.hasAttribute("xml:space"));
}

TEST(Text, SetContentNoBoundaryWhitespaceDoesNotSetXmlSpace) {
    Text text;
    text.setContent(std::make_unique<DomText>("normal text"));

    EXPECT_FALSE(text.hasAttribute("xml:space"));
}

TEST(Text, SetContentEmptyDoesNotSetXmlSpace) {
    Text text;
    text.setContent(std::make_unique<DomText>(""));

    EXPECT_FALSE(text.hasAttribute("xml:space"));
}

// ================================================================
// Section
// ================================================================

TEST(Section, Construction) {
    Section section;
    EXPECT_EQ(section.localName(), "sectPr");
    EXPECT_EQ(section.nodeType(), DomNodeType::Element);
    EXPECT_FALSE(section.namespaceUri().empty());
}

// ================================================================
// DOM 集成测试：构建完整文档树
// ================================================================

TEST(DomWordIntegration, BuildMinimalWordDocument) {
    auto doc = std::make_unique<WordDocument>();
    auto body = std::make_unique<Body>();
    auto* bodyRaw = body.get();

    auto para = std::make_unique<Paragraph>();
    auto run = std::make_unique<oso::Run>();
    auto text = std::make_unique<Text>();
    text->setContent(std::make_unique<DomText>("Test content"));
    run->addText(std::move(text));
    para->addRun(std::move(run));
    body->addParagraph(std::move(para));
    doc->setBody(std::move(body));

    // 验证结构
    EXPECT_EQ(doc->body(), bodyRaw);

    auto* paraObj = doc->body()->lastParagraph();
    ASSERT_NE(paraObj, nullptr);

    auto* runObj = paraObj->lastRun();
    ASSERT_NE(runObj, nullptr);

    EXPECT_EQ(runObj->textContent(), "Test content");
}

TEST(DomWordIntegration, SerializeRoundTripWordDocument) {
    // 1. 构建文档
    auto doc = std::make_unique<WordDocument>();
    auto body = std::make_unique<Body>();

    auto para = std::make_unique<Paragraph>();
    para->setAttribute("w:rsidR", "00AB1234");

    auto run = std::make_unique<oso::Run>();
    auto text = std::make_unique<Text>();
    text->setContent(std::make_unique<DomText>("Round-trip content"));
    run->addText(std::move(text));
    para->addRun(std::move(run));
    body->addParagraph(std::move(para));
    doc->setBody(std::move(body));

    // 2. 序列化
    MemoryStream stream;
    Libxml2Writer writer;
    writer.setOutput(stream);
    doc->serialize(writer);

    // 3. 解析回
    Libxml2Reader reader;
    std::string capturedText;
    std::vector<std::string> elementNames;
    std::string paraAttrs;

    reader.parse(stream.data(),
        [&](const std::string&, const std::string& localName,
            const std::string&, const std::vector<XmlAttribute>& attrs) {
            elementNames.push_back(localName);
            if (localName == "p") {
                for (const auto& a : attrs) {
                    paraAttrs += a.localName + "=" + a.value + ";";
                }
            }
        },
        nullptr,
        [&](const std::string& text) {
            bool isSpace=true;
            for(const auto& c:text){
                if(!isspace(c))isSpace=false;
            }
            if(isSpace)return;
            capturedText += text;
        });

    // 4. 验证
    ASSERT_GE(elementNames.size(), 5u); // document, body, p, r, t
    EXPECT_EQ(capturedText, "Round-trip content");
}

TEST(DomWordIntegration, MultiParagraphMultiRunDocument) {
    auto doc = std::make_unique<WordDocument>();
    auto body = std::make_unique<Body>();

    // 第一个段落：两个 Run
    auto para1 = std::make_unique<Paragraph>();
    auto r1 = std::make_unique<oso::Run>();
    auto t1 = std::make_unique<Text>();
    t1->setContent(std::make_unique<DomText>("Hello"));
    r1->addText(std::move(t1));
    para1->addRun(std::move(r1));

    auto r2 = std::make_unique<oso::Run>();
    auto t2 = std::make_unique<Text>();
    t2->setContent(std::make_unique<DomText>(" World"));
    r2->addText(std::move(t2));
    para1->addRun(std::move(r2));
    body->addParagraph(std::move(para1));

    // 第二个段落：一个 Run
    auto para2 = std::make_unique<Paragraph>();
    auto r3 = std::make_unique<oso::Run>();
    auto t3 = std::make_unique<Text>();
    t3->setContent(std::make_unique<DomText>("Second paragraph"));
    r3->addText(std::move(t3));
    para2->addRun(std::move(r3));
    body->addParagraph(std::move(para2));

    doc->setBody(std::move(body));

    // 验证
    ASSERT_NE(doc->body(), nullptr);
    auto paras = doc->body()->paragraphs();
    ASSERT_EQ(paras.size(), 2u);

    EXPECT_EQ(paras[0]->runs().size(), 2u);
    EXPECT_EQ(paras[1]->runs().size(), 1u);

    EXPECT_EQ(paras[0]->runs()[0]->textContent(), "Hello");
    EXPECT_EQ(paras[0]->runs()[1]->textContent(), " World");
    EXPECT_EQ(paras[1]->runs()[0]->textContent(), "Second paragraph");
}
