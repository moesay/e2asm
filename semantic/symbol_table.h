/**
 * @file symbol_table.h
 * @brief Symbol table for tracking labels, constants, and variables
 *
 * The symbol table maps names to addresses and values during assembly.
 * Supports case-insensitive lookup, local label scoping, and multi-pass
 * symbol resolution for forward references.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <cstdint>
#include <algorithm>
#include <cctype>

namespace e2asm {

/**
 * @brief Category of symbol in the program
 */
enum class SymbolType {
    LABEL,      ///< Code or data position marker (gets an address)
    CONSTANT,   ///< EQU-defined constant (purely compile-time)
    VARIABLE    ///< Reserved space (future use)
};

/**
 * @brief A single symbol with its properties
 *
 * Represents a named entity in the assembly program. Labels resolve to memory
 * addresses, constants hold compile-time values from EQU directives.
 */
struct Symbol {
    std::string name;           ///< Symbol identifier
    SymbolType type;            ///< What kind of symbol this is
    int64_t value;              ///< Address for labels, value for constants
    bool is_resolved;           ///< Whether value is final (handles forward refs)
    size_t definition_line;     ///< Source line where defined

    Symbol() : type(SymbolType::LABEL), value(0), is_resolved(false), definition_line(0) {}

    Symbol(std::string n, SymbolType t, int64_t val, size_t line)
        : name(std::move(n))
        , type(t)
        , value(val)
        , is_resolved(true)
        , definition_line(line)
    {}
};

/**
 * @brief Hash functor for case-insensitive string hashing
 *
 * Allows the symbol table to treat "Start", "START", and "start" as the same
 * identifier, matching traditional assembler behavior.
 */
struct CaseInsensitiveHash {
    size_t operator()(const std::string& str) const {
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return std::hash<std::string>()(lower);
    }
};

/**
 * @brief Equality functor for case-insensitive string comparison
 */
struct CaseInsensitiveEqual {
    bool operator()(const std::string& a, const std::string& b) const {
        if (a.length() != b.length()) return false;
        return std::equal(a.begin(), a.end(), b.begin(), b.end(),
            [](char ca, char cb) { return std::tolower(ca) == std::tolower(cb); });
    }
};

/**
 * @brief Symbol table managing labels, constants, and their scopes
 *
 * Provides case-insensitive symbol storage with support for local labels.
 * Local labels (starting with '.') are scoped to the most recent global label,
 * allowing reuse of names like ".loop" in different functions.
 *
 * The table supports multi-pass assembly where symbols can be defined as
 * unresolved initially and filled in later when their addresses are known.
 *
 * Example of local label scoping:
 * @code
 * start:           ; Global label, starts new scope
 *   mov ax, 1
 * .loop:           ; Local to 'start', becomes 'start.loop'
 *   inc ax
 *   jmp .loop
 *
 * done:            ; New global label, new scope
 *   xor ax, ax
 * .loop:           ; Local to 'done', becomes 'done.loop' (different from start.loop)
 *   dec ax
 *   jmp .loop
 * @endcode
 */
class SymbolTable {
public:
    SymbolTable() = default;

    /**
     * @brief Adds a new symbol to the table
     * @param name Symbol name (case-insensitive)
     * @param type Symbol category
     * @param value Initial value (address or constant)
     * @param line Source line where defined
     * @return true if added successfully, false if already exists
     *
     * Local labels (starting with '.') are automatically qualified with
     * the current global scope.
     */
    bool define(const std::string& name, SymbolType type, int64_t value, size_t line);

    /**
     * @brief Changes the value of an existing symbol
     * @param name Symbol to update
     * @param new_value New value or address
     * @return true if updated, false if symbol doesn't exist
     *
     * Used during multi-pass assembly when addresses change as instruction
     * sizes are refined.
     */
    bool update(const std::string& name, int64_t new_value);

    /**
     * @brief Marks an unresolved symbol as resolved with a final value
     * @param name Symbol to resolve
     * @param value Final value or address
     * @return true if resolved, false if doesn't exist or already resolved
     *
     * Used for forward references: first pass creates unresolved placeholders,
     * later passes fill in actual addresses.
     */
    bool resolve(const std::string& name, int64_t value);

    /**
     * @brief Looks up a symbol, handling local label scoping
     * @param name Symbol to find (may be local like ".loop")
     * @return Symbol if found, nullopt otherwise
     *
     * Automatically qualifies local labels with current scope before lookup.
     */
    std::optional<Symbol> lookup(const std::string& name) const;

    /**
     * @brief Looks up a symbol by exact name without scoping
     * @param name Exact symbol name to find
     * @return Symbol if found, nullopt otherwise
     *
     * Use when you need to find a fully-qualified name directly.
     */
    std::optional<Symbol> lookupDirect(const std::string& name) const;

    /**
     * @brief Checks if a symbol exists
     * @param name Symbol to check (handles local label scoping)
     * @return true if symbol is defined
     */
    bool exists(const std::string& name) const;

    /**
     * @brief Gets all symbols for iteration
     * @return Map of all symbols (case-insensitive)
     */
    const std::unordered_map<std::string, Symbol, CaseInsensitiveHash, CaseInsensitiveEqual>&
    getAllSymbols() const { return m_symbols; }

    /**
     * @brief Removes all symbols and resets scope
     *
     * Call between assembly runs to reuse the same table instance.
     */
    void clear() {
        m_symbols.clear();
        m_current_global_label.clear();
    }

    /**
     * @brief Sets the current global label for local label scoping
     * @param global_label Name of enclosing global label
     *
     * Called when encountering a new global label. All subsequent local labels
     * will be qualified with this name.
     */
    void setGlobalScope(const std::string& global_label) {
        m_current_global_label = global_label;
    }

    /**
     * @brief Gets the current global scope name
     * @return Current global label, or empty if none
     */
    const std::string& getGlobalScope() const {
        return m_current_global_label;
    }

    /**
     * @brief Qualifies a local label with current global scope
     * @param label Label name (may be local or global)
     * @return Fully qualified name (e.g., "start.loop" for ".loop")
     *
     * Non-local labels are returned unchanged. Local labels get prefixed
     * with the current global scope.
     */
    std::string getFullyQualifiedName(const std::string& label) const {
        if (!label.empty() && label[0] == '.' && !m_current_global_label.empty()) {
            return m_current_global_label + label;
        }
        return label;
    }

    /**
     * @brief Checks if a label is local (starts with '.')
     * @param label Label name to check
     * @return true if label starts with '.'
     */
    static bool isLocalLabel(const std::string& label) {
        return !label.empty() && label[0] == '.';
    }

private:
    /// Symbol storage with case-insensitive keys
    std::unordered_map<std::string, Symbol, CaseInsensitiveHash, CaseInsensitiveEqual> m_symbols;

    /// Current global label for local label scoping
    std::string m_current_global_label;
};

} // namespace e2asm
