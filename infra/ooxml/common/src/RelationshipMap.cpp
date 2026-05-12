#include "oso/ooxml/common/RelationshipMap.h"
#include "oso/ooxml/common/OoxmlNamespaces.h"
#include "oso/base/ErrorCode.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <cstring>
#include <sstream>

namespace oso {

std::vector<uint8_t> RelationshipMap::generate(
    const std::vector<Relationship>& relationships) {

    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        << "<Relationships xmlns=\"" << OoxmlNamespaces::kRelationships << "\">\n";

    for (const auto& rel : relationships) {
        xml << "  <Relationship Id=\"" << rel.id
            << "\" Type=\"" << rel.type
            << "\" Target=\"" << rel.target << "\"";
        if (rel.isExternal) {
            xml << " TargetMode=\"External\"";
        }
        xml << "/>\n";
    }

    xml << "</Relationships>\n";

    std::string s = xml.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

Result<RelationshipMap> RelationshipMap::parse(const std::vector<uint8_t>& xmlData) {
    if (xmlData.empty()) {
        return Result<RelationshipMap>::err(ErrorCode::OOXML_XmlParseError, "Empty rels XML data");
    }

    xmlDocPtr doc = xmlReadMemory(
        reinterpret_cast<const char*>(xmlData.data()),
        static_cast<int>(xmlData.size()),
        nullptr, nullptr,
        XML_PARSE_NOBLANKS | XML_PARSE_NONET | XML_PARSE_COMPACT);

    if (!doc) {
        return Result<RelationshipMap>::err(ErrorCode::OOXML_XmlParseError, "Failed to parse rels XML");
    }

    RelationshipMap map;

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root || std::strcmp(reinterpret_cast<const char*>(root->name), "Relationships") != 0) {
        xmlFreeDoc(doc);
        return Result<RelationshipMap>::err(ErrorCode::OOXML_InvalidSchema,
            "Rels root element is not <Relationships>");
    }

    for (xmlNodePtr child = root->children; child; child = child->next) {
        if (child->type != XML_ELEMENT_NODE) continue;

        if (std::strcmp(reinterpret_cast<const char*>(child->name), "Relationship") != 0) {
            continue;
        }

        xmlChar* id = xmlGetProp(child, reinterpret_cast<const xmlChar*>("Id"));
        xmlChar* type = xmlGetProp(child, reinterpret_cast<const xmlChar*>("Type"));
        xmlChar* target = xmlGetProp(child, reinterpret_cast<const xmlChar*>("Target"));
        xmlChar* targetMode = xmlGetProp(child, reinterpret_cast<const xmlChar*>("TargetMode"));

        if (!id || !type || !target) {
            if (id) xmlFree(id);
            if (type) xmlFree(type);
            if (target) xmlFree(target);
            if (targetMode) xmlFree(targetMode);
            continue;
        }

        Relationship rel;
        rel.id = reinterpret_cast<const char*>(id);
        rel.type = reinterpret_cast<const char*>(type);
        rel.target = reinterpret_cast<const char*>(target);
        rel.isExternal = (targetMode && std::strcmp(reinterpret_cast<const char*>(targetMode), "External") == 0);

        map.m_byId[rel.id] = map.m_relationships.size();
        map.m_relationships.push_back(std::move(rel));

        xmlFree(id);
        xmlFree(type);
        xmlFree(target);
        if (targetMode) xmlFree(targetMode);
    }

    xmlFreeDoc(doc);
    return Result<RelationshipMap>::ok(std::move(map));
}

Result<const RelationshipMap::Relationship*> RelationshipMap::getById(std::string_view id) const {
    auto it = m_byId.find(std::string(id));
    if (it == m_byId.end()) {
        return Result<const Relationship*>::err(ErrorCode::OOXML_MissingPart,
            "Relationship not found: " + std::string(id));
    }
    return Result<const Relationship*>::ok(&m_relationships[it->second]);
}

Result<const RelationshipMap::Relationship*> RelationshipMap::getByTarget(std::string_view target) const{
    for (const auto& rel : m_relationships) {
        if (rel.target == target) {
            return Result<const Relationship*>::ok(&rel);
        }
    }
    return Result<const Relationship*>::err(ErrorCode::OOXML_MissingPart,
        "Relationship not found for target: " + std::string(target));
}

std::vector<const RelationshipMap::Relationship*> RelationshipMap::getAllByType(std::string_view type) const {
    std::vector<const Relationship*> result;
    for (const auto& rel : m_relationships) {
        if (rel.type == type) {
            result.push_back(&rel);
        }
    }
    return result;
}

} // namespace oso
