#include "expression_parser.h"
#include "ast.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace e2asm {

// Helper method moved from header
std::string addressExprToString(const AddressExpression& expr) {
    std::string result = "[";
    for (size_t i = 0; i < expr.registers.size(); i++) {
        if (i > 0) result += "+";
        result += expr.registers[i];
    }
    if (expr.has_displacement) {
        if (!expr.registers.empty()) result += "+";
        result += std::to_string(expr.displacement);
    }
    result += "]";
    return result;
}

// Valid addressing registers
static const std::vector<std::string> VALID_ADDRESSING_REGS = {
    "BX", "BP", "SI", "DI"
};

std::optional<AddressExpression> ExpressionParser::parseAddress(const std::string& expr) {
    AddressExpression result;

    // Split by '+' and '-'
    std::string current;
    std::vector<std::string> parts;
    bool is_negative = false;

    for (size_t i = 0; i < expr.length(); i++) {
        char c = expr[i];

        if (c == '+' || c == '-') {
            if (!current.empty()) {
                // Trim whitespace
                current.erase(0, current.find_first_not_of(" \t"));
                current.erase(current.find_last_not_of(" \t") + 1);

                if (!current.empty()) {
                    if (is_negative) {
                        current = "-" + current;
                    }
                    parts.push_back(current);
                }
                current.clear();
            }
            is_negative = (c == '-');
        } else {
            current += c;
        }
    }

    // Add last part
    if (!current.empty()) {
        current.erase(0, current.find_first_not_of(" \t"));
        current.erase(current.find_last_not_of(" \t") + 1);
        if (!current.empty()) {
            if (is_negative) {
                current = "-" + current;
            }
            parts.push_back(current);
        }
    }

    // Classify each part as register, number, or label
    for (const auto& part : parts) {
        if (isRegister(part)) {
            result.registers.push_back(normalizeRegister(part));
        } else {
            // Try to parse as number
            auto num = parseNumber(part);
            if (num) {
                result.displacement += *num;
                result.has_displacement = true;
            } else {
                // Treat as label/symbol reference
                // Check if it's a valid identifier (letters, digits, underscore, dot)
                bool is_valid_identifier = !part.empty();
                for (char c : part) {
                    if (!std::isalnum(c) && c != '_' && c != '.') {
                        is_valid_identifier = false;
                        break;
                    }
                }

                if (is_valid_identifier && std::isalpha(part[0])) {
                    // Valid label reference
                    result.label_name = part;
                    result.has_label = true;
                } else {
                    // Invalid part
                    return std::nullopt;
                }
            }
        }
    }

    return result;
}

std::optional<int64_t> ExpressionParser::evaluate(const std::string& expr) {
    // Use arithmetic evaluator for Phase 8
    return evaluateArithmetic(expr);
}

std::optional<int64_t> ExpressionParser::evaluateWithContext(
    const std::string& expr,
    uint64_t current_pos,
    uint64_t segment_start
) {
    // Replace $ and $$ with actual values
    std::string processed_expr = expr;

    // Replace $$ first (to avoid confusion with $)
    size_t pos = 0;
    while ((pos = processed_expr.find("$$", pos)) != std::string::npos) {
        processed_expr.replace(pos, 2, std::to_string(segment_start));
        pos += std::to_string(segment_start).length();
    }

    // Replace single $
    pos = 0;
    while ((pos = processed_expr.find('$', pos)) != std::string::npos) {
        processed_expr.replace(pos, 1, std::to_string(current_pos));
        pos += std::to_string(current_pos).length();
    }

    // Now evaluate the expression with basic arithmetic
    return evaluateArithmetic(processed_expr);
}

std::optional<int64_t> ExpressionParser::evaluateArithmetic(const std::string& expr) {
    // Remove whitespace
    std::string clean;
    for (char c : expr) {
        if (!std::isspace(c)) clean += c;
    }

    if (clean.empty()) return std::nullopt;

    // Parse addition/subtraction (lowest precedence, right-to-left scan)
    for (size_t i = clean.length(); i > 0; i--) {
        char c = clean[i-1];
        // Skip if this is a negative sign at the start or after an operator
        if (i == 1 || (c == '-' && (clean[i-2] == '+' || clean[i-2] == '-' ||
                                     clean[i-2] == '*' || clean[i-2] == '/' ||
                                     clean[i-2] == '('))) {
            continue;
        }

        if (c == '+' || c == '-') {
            auto left = evaluateArithmetic(clean.substr(0, i-1));
            auto right = evaluateArithmetic(clean.substr(i));
            if (!left || !right) return std::nullopt;

            return (c == '+') ? (*left + *right) : (*left - *right);
        }
    }

    // Parse multiplication/division (higher precedence, right-to-left scan)
    for (size_t i = clean.length(); i > 0; i--) {
        char c = clean[i-1];
        if (c == '*' || c == '/') {
            auto left = evaluateArithmetic(clean.substr(0, i-1));
            auto right = evaluateArithmetic(clean.substr(i));
            if (!left || !right) return std::nullopt;
            if (c == '/' && *right == 0) return std::nullopt;  // Division by zero

            return (c == '*') ? (*left * *right) : (*left / *right);
        }
    }

    // Handle parentheses
    if (clean.front() == '(' && clean.back() == ')') {
        return evaluateArithmetic(clean.substr(1, clean.length() - 2));
    }

    // Base case: parse as number
    return parseNumber(clean);
}

std::optional<int64_t> ExpressionParser::parseNumber(const std::string& str) {
    if (str.empty()) return std::nullopt;

    std::string s = str;
    bool is_negative = false;

    // Handle negative sign
    if (s[0] == '-') {
        is_negative = true;
        s = s.substr(1);
    }

    try {
        int64_t value = 0;

        // Check for hex prefix
        if (s.length() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            value = std::stoll(s.substr(2), nullptr, 16);
        }
        // Check for hex suffix
        else if (s.length() >= 2 && (s.back() == 'h' || s.back() == 'H')) {
            value = std::stoll(s.substr(0, s.length() - 1), nullptr, 16);
        }
        // Check for binary prefix
        else if (s.length() >= 2 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
            value = std::stoll(s.substr(2), nullptr, 2);
        }
        // Check for binary suffix
        else if (s.length() >= 2 && (s.back() == 'b' || s.back() == 'B')) {
            value = std::stoll(s.substr(0, s.length() - 1), nullptr, 2);
        }
        // Check for octal
        else if (s.length() >= 2 && s[0] == '0' && (s[1] == 'o' || s[1] == 'O')) {
            value = std::stoll(s.substr(2), nullptr, 8);
        }
        // Decimal
        else {
            value = std::stoll(s, nullptr, 10);
        }

        return is_negative ? -value : value;
    } catch (...) {
        return std::nullopt;
    }
}

bool ExpressionParser::isValidIdentifier(const std::string& str) {
    if (str.empty()) return false;
    // Must start with letter or underscore
    if (!std::isalpha(str[0]) && str[0] != '_' && str[0] != '.') return false;
    // Rest can be alphanumeric, underscore, or dot
    for (char c : str) {
        if (!std::isalnum(c) && c != '_' && c != '.') return false;
    }
    return true;
}

std::optional<int64_t> ExpressionParser::evaluateWithSymbols(
    const std::string& expr,
    const SymbolLookupCallback& symbol_lookup
) {
    return evaluateArithmeticWithSymbols(expr, symbol_lookup);
}

std::optional<int64_t> ExpressionParser::evaluateArithmeticWithSymbols(
    const std::string& expr,
    const SymbolLookupCallback& symbol_lookup
) {
    // Remove whitespace
    std::string clean;
    for (char c : expr) {
        if (!std::isspace(c)) clean += c;
    }

    if (clean.empty()) return std::nullopt;

    // Parse addition/subtraction (lowest precedence, right-to-left scan)
    for (size_t i = clean.length(); i > 0; i--) {
        char c = clean[i-1];
        // Skip if this is a negative sign at the start or after an operator
        if (i == 1 || (c == '-' && (clean[i-2] == '+' || clean[i-2] == '-' ||
                                     clean[i-2] == '*' || clean[i-2] == '/' ||
                                     clean[i-2] == '('))) {
            continue;
        }

        if (c == '+' || c == '-') {
            auto left = evaluateArithmeticWithSymbols(clean.substr(0, i-1), symbol_lookup);
            auto right = evaluateArithmeticWithSymbols(clean.substr(i), symbol_lookup);
            if (!left || !right) return std::nullopt;

            return (c == '+') ? (*left + *right) : (*left - *right);
        }
    }

    // Parse multiplication/division (higher precedence, right-to-left scan)
    for (size_t i = clean.length(); i > 0; i--) {
        char c = clean[i-1];
        if (c == '*' || c == '/') {
            auto left = evaluateArithmeticWithSymbols(clean.substr(0, i-1), symbol_lookup);
            auto right = evaluateArithmeticWithSymbols(clean.substr(i), symbol_lookup);
            if (!left || !right) return std::nullopt;
            if (c == '/' && *right == 0) return std::nullopt;  // Division by zero

            return (c == '*') ? (*left * *right) : (*left / *right);
        }
    }

    // Handle parentheses
    if (clean.front() == '(' && clean.back() == ')') {
        return evaluateArithmeticWithSymbols(clean.substr(1, clean.length() - 2), symbol_lookup);
    }

    // Try to parse as number first
    auto num = parseNumber(clean);
    if (num) return num;

    // Try to resolve as symbol
    if (isValidIdentifier(clean) && symbol_lookup) {
        return symbol_lookup(clean);
    }

    return std::nullopt;
}

std::optional<AddressExpression> ExpressionParser::parseAddressWithSymbols(
    const std::string& expr,
    const SymbolLookupCallback& symbol_lookup
) {
    AddressExpression result;

    // First, extract any registers from the expression
    // We need to identify registers vs arithmetic parts
    std::string remaining_expr;
    std::string current;
    bool in_arithmetic = false;
    int paren_depth = 0;

    // Tokenize the expression more carefully
    std::vector<std::pair<std::string, bool>> tokens;  // token, is_negative
    bool next_negative = false;

    for (size_t i = 0; i < expr.length(); i++) {
        char c = expr[i];

        if (c == '(') paren_depth++;
        if (c == ')') paren_depth--;

        if (paren_depth == 0 && (c == '+' || c == '-')) {
            if (!current.empty()) {
                // Trim whitespace
                current.erase(0, current.find_first_not_of(" \t"));
                if (!current.empty() && current.find_last_not_of(" \t") != std::string::npos) {
                    current.erase(current.find_last_not_of(" \t") + 1);
                }
                if (!current.empty()) {
                    tokens.push_back({current, next_negative});
                }
                current.clear();
            }
            next_negative = (c == '-');
        } else {
            current += c;
        }
    }

    // Add last token
    if (!current.empty()) {
        current.erase(0, current.find_first_not_of(" \t"));
        if (!current.empty() && current.find_last_not_of(" \t") != std::string::npos) {
            current.erase(current.find_last_not_of(" \t") + 1);
        }
        if (!current.empty()) {
            tokens.push_back({current, next_negative});
        }
    }

    // Now process each token
    for (const auto& [token, is_negative] : tokens) {
        // Check if it's a simple register
        if (isRegister(token)) {
            if (is_negative) {
                // Can't have negative register
                return std::nullopt;
            }
            result.registers.push_back(normalizeRegister(token));
            continue;
        }

        // Try to evaluate as expression (number, symbol, or arithmetic)
        std::string eval_expr = is_negative ? ("-" + token) : token;

        // Check if it's purely a simple identifier (label reference)
        if (isValidIdentifier(token) && !isRegister(token)) {
            // Try to resolve it as a symbol first
            if (symbol_lookup) {
                auto resolved = symbol_lookup(token);
                if (resolved) {
                    // It's an EQU constant - add to displacement
                    int64_t value = is_negative ? -*resolved : *resolved;
                    result.displacement += value;
                    result.has_displacement = true;
                    continue;
                }
            }
            // Not found as symbol - treat as label reference
            if (result.has_label) {
                // Can't have multiple labels - try to evaluate as expression
                auto value = evaluateArithmeticWithSymbols(eval_expr, symbol_lookup);
                if (value) {
                    result.displacement += *value;
                    result.has_displacement = true;
                    continue;
                }
                return std::nullopt;  // Multiple unresolved labels
            }
            result.label_name = token;
            result.has_label = true;
            if (is_negative) {
                // Label with negative - not typical but mark it
                // We'll need to handle this in code generation
            }
            continue;
        }

        // Try to evaluate as arithmetic expression with symbols
        auto value = evaluateArithmeticWithSymbols(eval_expr, symbol_lookup);
        if (value) {
            result.displacement += *value;
            result.has_displacement = true;
            continue;
        }

        // If evaluation fails, it might contain an unresolved label
        // Try simpler parsing
        auto num = parseNumber(token);
        if (num) {
            int64_t value = is_negative ? -*num : *num;
            result.displacement += value;
            result.has_displacement = true;
            continue;
        }

        // Can't parse this token
        return std::nullopt;
    }

    return result;
}

bool ExpressionParser::isRegister(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    for (const auto& reg : VALID_ADDRESSING_REGS) {
        if (upper == reg) return true;
    }

    return false;
}

std::string ExpressionParser::normalizeRegister(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return upper;
}

} // namespace e2asm
