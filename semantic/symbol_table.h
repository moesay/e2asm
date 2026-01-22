#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <cstdint>
#include <algorithm>
#include <cctype>

namespace e2asm {

enum class SymbolType {
    LABEL,
    CONSTANT,
    VARIABLE
};

struct Symbol {
    std::string name;
    SymbolType type;
    int64_t value;          // Address or constant value
    bool is_resolved;       // Has value been determined?
    size_t definition_line;

    Symbol() : type(SymbolType::LABEL), value(0), is_resolved(false), definition_line(0) {}

    Symbol(std::string n, SymbolType t, int64_t val, size_t line)
        : name(std::move(n))
        , type(t)
        , value(val)
        , is_resolved(true)
        , definition_line(line)
    {}
};

struct CaseInsensitiveHash {
    size_t operator()(const std::string& str) const {
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return std::hash<std::string>()(lower);
    }
};

struct CaseInsensitiveEqual {
    bool operator()(const std::string& a, const std::string& b) const {
        if (a.length() != b.length()) return false;
        return std::equal(a.begin(), a.end(), b.begin(), b.end(),
            [](char ca, char cb) { return std::tolower(ca) == std::tolower(cb); });
    }
};

class SymbolTable {
public:
    SymbolTable() = default;

    bool define(const std::string& name, SymbolType type, int64_t value, size_t line);

    bool update(const std::string& name, int64_t new_value);

    bool resolve(const std::string& name, int64_t value);

    std::optional<Symbol> lookup(const std::string& name) const;

    std::optional<Symbol> lookupDirect(const std::string& name) const;

    bool exists(const std::string& name) const;

    const std::unordered_map<std::string, Symbol, CaseInsensitiveHash, CaseInsensitiveEqual>&
    getAllSymbols() const { return m_symbols; }

    void clear() {
        m_symbols.clear();
        m_current_global_label.clear();
    }

    void setGlobalScope(const std::string& global_label) {
        m_current_global_label = global_label;
    }

    const std::string& getGlobalScope() const {
        return m_current_global_label;
    }

    std::string getFullyQualifiedName(const std::string& label) const {
        if (!label.empty() && label[0] == '.' && !m_current_global_label.empty()) {
            return m_current_global_label + label;
        }
        return label;
    }

    static bool isLocalLabel(const std::string& label) {
        return !label.empty() && label[0] == '.';
    }

private:
    std::unordered_map<std::string, Symbol, CaseInsensitiveHash, CaseInsensitiveEqual> m_symbols;
    std::string m_current_global_label;
};

} // namespace e2asm
