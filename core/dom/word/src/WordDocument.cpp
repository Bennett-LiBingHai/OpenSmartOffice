#include "oso/dom/word/WordDocument.h"

#include "oso/ooxml/common/OoxmlNamespaces.h"
#include "oso/ooxml/write/IOoxmlWriter.h"

namespace oso {

// ============================================================
// WordDocument 实现
// ============================================================

WordDocument::WordDocument() : DomDocument("word") {
    m_localName = "document";
    m_namespaceUri = std::string(OoxmlNamespaces::kWordprocessingML);
}

Body* WordDocument::body() const {
    for (const auto& c : m_children) {
        if (auto* bd = dynamic_cast<Body*>(c.get())) {
            return bd;
        }
    }
    return nullptr;
}

void WordDocument::setBody(std::unique_ptr<Body> body) {
    for (auto& c : m_children) {
        if (dynamic_cast<Body*>(c.get())) {
            c = std::move(body);
            return;
        }
    }
    appendChild(std::move(body));
}

void WordDocument::serialize(IOoxmlWriter& writer) const {
    writer.writeStartDocument(true);
    serializeStartElement(writer);
    for (const auto& child : m_children) {
        child->serialize(writer);
    }
    serializeEndElement(writer);
    writer.writeEndDocument();
}

// ============================================================
// Body 实现
// ============================================================

Body::Body() : DomElement("body", std::string(OoxmlNamespaces::kWordprocessingML)) {
}

void Body::addParagraph(std::unique_ptr<Paragraph> para) {
    appendChild(std::move(para));
}

std::vector<Paragraph*> Body::paragraphs() const {
    std::vector<Paragraph*> result;
    for (const auto& child : m_children) {
        if (child->localName() == "p") {
            result.push_back(static_cast<Paragraph*>(child.get()));
        }
    }
    return result;
}

Paragraph* Body::lastParagraph() const {
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        if ((*it)->localName() == "p") {
            return static_cast<Paragraph*>((*it).get());
        }
    }
    return nullptr;
}

void Body::serialize(IOoxmlWriter& writer) const {
    DomElement::serialize(writer);
}

// ============================================================
// Paragraph 实现
// ============================================================

Paragraph::Paragraph() : DomElement("p", std::string(OoxmlNamespaces::kWordprocessingML)) {
}

void Paragraph::addRun(std::unique_ptr<Run> run) {
    appendChild(std::move(run));
}

std::vector<Run*> Paragraph::runs() const {
    std::vector<Run*> result;
    for (const auto& child : m_children) {
        if (child->localName() == "r") {
            result.push_back(static_cast<Run*>(child.get()));
        }
    }
    return result;
}

Run* Paragraph::lastRun() const {
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        if ((*it)->localName() == "r") {
            return static_cast<Run*>((*it).get());
        }
    }
    return nullptr;
}

void Paragraph::serialize(IOoxmlWriter& writer) const {
    DomElement::serialize(writer);
}

// ============================================================
// Run 实现
// ============================================================

Run::Run() : DomElement("r", std::string(OoxmlNamespaces::kWordprocessingML)) {
}

void Run::addText(std::unique_ptr<Text> text) {
    appendChild(std::move(text));
}

std::string Run::textContent() const {
    std::string result;
    for (const auto& child : m_children) {
        if (auto* textNode = dynamic_cast<const Text*>(child.get())) {
            result += textNode->content()->text();
        }
    }
    return result;
}

std::vector<Text*> Run::texts() const {
    std::vector<Text*> result;
    for (const auto& child : m_children) {
        if (auto* textNode = dynamic_cast<Text*>(child.get())) {
            result.push_back(textNode);
        }
    }
    return result;
}

void Run::serialize(IOoxmlWriter& writer) const {
    DomElement::serialize(writer);
}

// ============================================================
// Text 实现
// ============================================================

Text::Text() : DomElement("t", std::string(OoxmlNamespaces::kWordprocessingML)) {
    appendChild(std::make_unique<DomText>());
}

DomText* Text::content() const {
    for (const auto& child : m_children) {
        if (auto* content = dynamic_cast<DomText*>(child.get())) {
            return content;
        }
    }
    return nullptr;
}

void Text::setContent(std::unique_ptr<class DomText> content) {
    const std::string& s = content->text();
    if (!s.empty() && (isspace(s.front()) || isspace(s.back()))) {
        m_attributes["xml:space"] = "preserve";
    } else {
        m_attributes.erase("xml:space");
    }
    for (const auto& c : m_children) {
        if (auto* text = dynamic_cast<DomText*>(c.get())) {
            text->setText(s);
            return;
        }
    }
    appendChild(std::move(content));
}

void Text::serialize(IOoxmlWriter& writer) const {
    DomElement::serialize(writer);
}

// ============================================================
// Section 实现
// ============================================================

Section::Section() : DomElement("sectPr", std::string(OoxmlNamespaces::kWordprocessingML)) {
}

}  // namespace oso
