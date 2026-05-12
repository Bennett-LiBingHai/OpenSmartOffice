#pragma once

#include <string>
#include <string_view>
#include <cctype>
#include <utility>

namespace oso {

// ============================================================
// PartPath — OOXML 包内部件路径的轻量封装。
//
// 约定：空字符串表示"未设置"；非空路径总是以 "/" 开头，使用 "/" 分隔符。
//
// 注意：本类不执行完整路径合法性校验和规范化（不处理连续 "/"、".."、
// "\" 等）。调用者应确保传入的路径符合 OOXML 部件路径规范。
// ============================================================
class PartPath {
public:
    PartPath() = default;

    explicit PartPath(std::string path)
        : m_path(std::move(path))
    {
        if (!m_path.empty() && m_path[0] != '/') {
            m_path.insert(0, 1, '/');
        }
    }

    static PartPath fromString(std::string_view sv) {
        return PartPath(std::string(sv));
    }

    [[nodiscard]] const std::string& toString() const { return m_path; }
    [[nodiscard]] const char* c_str() const { return m_path.c_str(); }
    [[nodiscard]] bool empty() const { return m_path.empty(); }

    // 返回不含前导 "/" 的相对形式
    [[nodiscard]] std::string relativePath() const {
        if (!m_path.empty() && m_path[0] == '/') {
            return m_path.substr(1);
        }
        return m_path;
    }

    // 路径最后一段（文件名）
    [[nodiscard]] std::string name() const {
        auto pos = m_path.rfind('/');
        if (pos == std::string::npos) return m_path;
        return m_path.substr(pos + 1);
    }

    // 所在目录，以 "/" 结尾。空路径返回 ""
    [[nodiscard]] std::string directory() const {
        if (m_path.empty()) return "";
        auto pos = m_path.rfind('/');
        if (pos == std::string::npos) return "/";
        return m_path.substr(0, pos + 1);
    }

    // 扩展名（不含 "."），如 "xml"、"png"、"rels"
    [[nodiscard]] std::string extension() const {
        auto nameStr = name();
        auto pos = nameStr.rfind('.');
        if (pos == std::string::npos) return "";
        return nameStr.substr(pos + 1);
    }

    bool operator==(const PartPath& o) const { return m_path == o.m_path; }
    bool operator!=(const PartPath& o) const { return m_path != o.m_path; }
    bool operator<(const PartPath& o) const  { return m_path < o.m_path; }

private:
    std::string m_path;  ///< 部件路径，约定非空时以 "/" 开头
};

// ============================================================
// RelationshipId — OOXML 关系 ID（如 "rId4"）的轻量封装。
//
// 约定：空字符串表示"未设置"；非空 ID 通常格式为 "rIdN"（N > 0），
// 但本类不强制校验此约定，仅通过 isValid() / fromIndex() 提供辅助。
// ============================================================
class RelationshipId {
public:
    RelationshipId() = default;

    explicit RelationshipId(std::string id)
        : m_id(std::move(id)) {}

    static RelationshipId fromString(std::string_view sv) {
        return RelationshipId(std::string(sv));
    }

    // 从正整数序号创建。index <= 0 时返回空值。
    [[nodiscard]] static RelationshipId fromIndex(int index) {
        if (index <= 0) return {};
        return RelationshipId("rId" + std::to_string(index));
    }

    [[nodiscard]] const std::string& toString() const { return m_id; }
    [[nodiscard]] const char* c_str() const { return m_id.c_str(); }
    [[nodiscard]] bool empty() const { return m_id.empty(); }

    // 是否为合法的 "rIdN" 格式（N > 0）
    [[nodiscard]] bool isValid() const { return index() > 0; }

    // 提取数字索引（如 "rId4" → 4），无效格式返回 -1
    [[nodiscard]] int index() const {
        if (m_id.size() < 4 || m_id[0] != 'r' || m_id[1] != 'I' || m_id[2] != 'd') {
            return -1;
        }
        int value = 0;
        for (size_t i = 3; i < m_id.size(); ++i) {
            char c = m_id[i];
            if (c < '0' || c > '9') return -1;
            value = value * 10 + (c - '0');
        }
        return value;
    }

    bool operator==(const RelationshipId& o) const { return m_id == o.m_id; }
    bool operator!=(const RelationshipId& o) const { return m_id != o.m_id; }
    bool operator<(const RelationshipId& o) const { return m_id < o.m_id; }

private:
    std::string m_id;  ///< 关系 ID 字符串，如 "rId4"
};

// ============================================================
// ExternalReference — 指向 OOXML 包外资源的链接。
//
// 本类做粗略的 URI 类型识别，不做完整 RFC 3986 校验。
// 不支持 scheme 的纯路径被归类为 File。
// ============================================================
class ExternalReference {
public:
    enum class Type {
        Url,       // http://, https://
        File,      // file://、绝对/相对文件路径、无 scheme 的路径
        Mailto,    // mailto:
        Other,     // 其他 URI scheme（ftp:, urn:, data:, tel: 等）
    };

    ExternalReference() = default;

    explicit ExternalReference(std::string uri)
        : m_uri(std::move(uri))
    {
        detectType();
    }

    static ExternalReference fromString(std::string_view sv) {
        return ExternalReference(std::string(sv));
    }

    [[nodiscard]] const std::string& uri() const { return m_uri; }
    [[nodiscard]] const char* c_str() const { return m_uri.c_str(); }
    [[nodiscard]] Type type() const { return m_type; }
    [[nodiscard]] bool empty() const { return m_uri.empty(); }

    // 是否包含 URI scheme（如 http:, file:, mailto:）
    [[nodiscard]] bool hasScheme() const {
        return hasUriScheme(m_uri);
    }

    // 是否为相对引用（非绝对、无 scheme）
    [[nodiscard]] bool isRelative() const {
        return !m_uri.empty() && !isAbsolute();
    }

    // 是否为绝对引用（有 scheme 或以 "//" 开头）
    [[nodiscard]] bool isAbsolute() const {
        return hasUriScheme(m_uri) || m_uri.starts_with("//");
    }

    bool operator==(const ExternalReference& o) const { return m_uri == o.m_uri; }
    bool operator!=(const ExternalReference& o) const { return m_uri != o.m_uri; }

private:
    // RFC 3986 §3.1: scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    static bool hasUriScheme(std::string_view s) {
        if (s.empty()) return false;
        if (!std::isalpha(static_cast<unsigned char>(s[0]))) return false;

        for (size_t i = 1; i < s.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (c == ':') return true;
            if (!(std::isalnum(c) || c == '+' || c == '-' || c == '.')) return false;
        }
        return false;
    }

    // Windows 盘符（如 "C:"）会被 RFC 3986 scheme 模式误匹配，
    // 需要排除：单字母 + ":" 不是合法 URI scheme。
    static bool isWindowsDrivePrefix(std::string_view s) {
        return s.size() >= 2
            && std::isalpha(static_cast<unsigned char>(s[0]))
            && s[1] == ':';
    }

    void detectType() {
        if (m_uri.starts_with("http://") || m_uri.starts_with("https://")) {
            m_type = Type::Url;
        } else if (m_uri.starts_with("mailto:")) {
            m_type = Type::Mailto;
        } else if (m_uri.starts_with("file://")) {
            m_type = Type::File;
        } else if (isWindowsDrivePrefix(m_uri) || !hasUriScheme(m_uri)) {
            // 无 scheme 的路径，或 Windows 盘符开头的路径
            m_type = Type::File;
        } else {
            m_type = Type::Other;
        }
    }

    std::string m_uri;         ///< 外部资源 URI 字符串
    Type m_type = Type::Other;  ///< 识别出的 URI 类型
};

} // namespace oso
