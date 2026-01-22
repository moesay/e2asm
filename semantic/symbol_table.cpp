#include "symbol_table.h"

namespace e2asm {

bool SymbolTable::define(const std::string& name, SymbolType type, int64_t value, size_t line) {
    std::string qualified_name = getFullyQualifiedName(name);

    if (exists(qualified_name)) {
        return false;
    }

    Symbol symbol(name, type, value, line);
    m_symbols[qualified_name] = symbol;
    return true;
}

bool SymbolTable::update(const std::string& name, int64_t new_value) {
    std::string qualified_name = getFullyQualifiedName(name);
    auto it = m_symbols.find(qualified_name);
    if (it == m_symbols.end()) {
        return false;
    }

    it->second.value = new_value;
    return true;
}

bool SymbolTable::resolve(const std::string& name, int64_t value) {
    std::string qualified_name = getFullyQualifiedName(name);
    auto it = m_symbols.find(qualified_name);
    if (it == m_symbols.end()) {
        return false;
    }

    it->second.value = value;
    it->second.is_resolved = true;
    return true;
}

std::optional<Symbol> SymbolTable::lookup(const std::string& name) const {
    std::string qualified_name = getFullyQualifiedName(name);
    auto it = m_symbols.find(qualified_name);
    if (it == m_symbols.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<Symbol> SymbolTable::lookupDirect(const std::string& name) const {
    auto it = m_symbols.find(name);
    if (it == m_symbols.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool SymbolTable::exists(const std::string& name) const {
    std::string qualified_name = getFullyQualifiedName(name);
    return m_symbols.find(qualified_name) != m_symbols.end();
}

} // namespace e2asm
