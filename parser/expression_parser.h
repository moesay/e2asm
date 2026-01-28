#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <functional>
#include "../lexer/token.h"

namespace e2asm {

// Forward declaration (actual definition is in ast.h to avoid circular dependencies)
struct AddressExpression;

/**
 * Callback type for symbol lookup during expression evaluation
 * Returns the symbol's value if found, nullopt otherwise
 */
using SymbolLookupCallback = std::function<std::optional<int64_t>(const std::string&)>;

/**
 * Expression parser for memory addresses and arithmetic
 */
class ExpressionParser {
public:
    /**
     * Parse memory address expression from tokens
     * Example: BX+SI+10 → {registers: ["BX", "SI"], displacement: 16}
     */
    static std::optional<AddressExpression> parseAddress(const std::string& expr);

    /**
     * Parse memory address expression with symbol resolution
     * Resolves EQU constants and labels in expressions like [label + const * 2 - 2]
     * @param expr Expression string
     * @param symbol_lookup Callback to resolve symbol names to values
     * @return Parsed address or nullopt on error
     */
    static std::optional<AddressExpression> parseAddressWithSymbols(
        const std::string& expr,
        const SymbolLookupCallback& symbol_lookup
    );

    /**
     * Evaluate simple arithmetic expression to constant
     * Example: "1+2*3" → 7
     */
    static std::optional<int64_t> evaluate(const std::string& expr);

    /**
     * Evaluate expression with symbol resolution
     * @param expr Expression string
     * @param symbol_lookup Callback to resolve symbol names to values
     * @return Evaluated value or nullopt on error
     */
    static std::optional<int64_t> evaluateWithSymbols(
        const std::string& expr,
        const SymbolLookupCallback& symbol_lookup
    );

    /**
     * Evaluate expression with position markers ($, $$)
     * @param expr Expression string
     * @param current_pos Current assembly position ($)
     * @param segment_start Segment start position ($$)
     * @return Evaluated value or nullopt on error
     */
    static std::optional<int64_t> evaluateWithContext(
        const std::string& expr,
        uint64_t current_pos,
        uint64_t segment_start
    );

private:
    /**
     * Evaluate arithmetic expression with +, -, *, / operators
     * Handles operator precedence (*, / before +, -)
     */
    static std::optional<int64_t> evaluateArithmetic(const std::string& expr);

    /**
     * Evaluate arithmetic expression with symbol lookup
     */
    static std::optional<int64_t> evaluateArithmeticWithSymbols(
        const std::string& expr,
        const SymbolLookupCallback& symbol_lookup
    );

    /**
     * Parse a number from string (handles hex, binary, octal)
     */
    static std::optional<int64_t> parseNumber(const std::string& str);

    /**
     * Check if string is a register name
     */
    static bool isRegister(const std::string& str);

    /**
     * Check if string is a valid identifier (for symbols/labels)
     */
    static bool isValidIdentifier(const std::string& str);

    /**
     * Normalize register name to uppercase
     */
    static std::string normalizeRegister(const std::string& str);
};

} // namespace e2asm
