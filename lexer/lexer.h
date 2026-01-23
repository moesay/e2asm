/**
 * @file lexer.h
 * @brief Lexical analyzer (tokenizer) for 8086 assembly
 *
 * The lexer is the first compilation phase. It reads raw source text and breaks
 * it into meaningful tokens like registers, numbers, keywords, and operators.
 * Handles Intel syntax with support for various number formats, string literals,
 * and assembly-specific constructs.
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "token.h"
#include "source_location.h"

namespace e2asm {

/**
 * @brief Converts assembly source text into a stream of tokens
 *
 * The lexer recognizes:
 * - Numbers in multiple formats: decimal (42), hex (0x2A, 2Ah), binary (0b101010), octal (52o)
 * - Identifiers and labels
 * - Register names (AL, AX, ES, etc.)
 * - Instructions (MOV, ADD, JMP, etc.)
 * - Directives (DB, DW, ORG, SEGMENT, etc.)
 * - Preprocessor directives (%include, %define, %macro, etc.)
 * - String and character literals with escape sequences
 * - Operators and punctuation
 * - Comments (line comments with ';')
 *
 * The lexer is designed to be fast and use minimal memory via string_view.
 */
class Lexer {
public:
    /**
     * @brief Constructs a lexer for the given source
     * @param source Assembly source code (must outlive the Lexer)
     * @param filename Name to display in error locations
     */
    explicit Lexer(std::string_view source, std::string filename = "<input>");

    /**
     * @brief Scans the entire source and produces all tokens
     * @return Vector of tokens including a final END_OF_FILE token
     *
     * Skips whitespace and comments automatically. Returns INVALID tokens
     * for unrecognized characters rather than throwing exceptions, allowing
     * the parser to report multiple errors.
     */
    std::vector<Token> tokenize();

private:
    /** @brief Scans and returns the next token, advancing position */
    Token nextToken();

    /** @brief Scans a numeric literal in any supported base */
    Token scanNumber();

    /** @brief Scans an identifier, keyword, register, or instruction */
    Token scanIdentifier();

    /** @brief Scans a quoted string literal, processing escape sequences */
    Token scanString();

    /** @brief Scans a single-quoted character literal */
    Token scanCharacter();

    /** @brief Checks if we've consumed all input */
    bool isAtEnd() const;

    /** @brief Returns current character without advancing */
    char peek() const;

    /** @brief Returns next character without advancing */
    char peekNext() const;

    /** @brief Consumes and returns current character */
    char advance();

    /** @brief Consumes current char if it matches, returns true if matched */
    bool match(char expected);

    /** @brief Skips spaces and tabs (but not newlines) */
    void skipWhitespace();

    /** @brief Skips from ';' to end of line */
    void skipLineComment();

    /** @brief Creates a SourceLocation for the current position */
    SourceLocation currentLocation() const;

    /** @brief Updates line/column tracking after consuming a character */
    void advanceLocation(char c);

    /** @brief Checks if character is a decimal digit */
    bool isDigit(char c) const;

    /** @brief Checks if character is a hexadecimal digit */
    bool isHexDigit(char c) const;

    /** @brief Checks if character can start an identifier */
    bool isAlpha(char c) const;

    /** @brief Checks if character can appear in an identifier */
    bool isAlphaNumeric(char c) const;

    /** @brief Determines if an identifier is a keyword or directive */
    TokenType identifierType(const std::string& text) const;

    /** @brief Determines if an identifier is a register name */
    TokenType registerType(const std::string& text) const;

    std::string_view m_source;  ///< Source text (not owned, must outlive lexer)
    std::string m_filename;     ///< Filename for error reporting
    size_t m_current;           ///< Current position in source
    size_t m_line;              ///< Current line number (1-based)
    size_t m_column;            ///< Current column number (1-based)

    /** @brief Maps directive names to their token types */
    static const std::unordered_map<std::string, TokenType> s_keywords;

    /** @brief Maps register names to their token types */
    static const std::unordered_map<std::string, TokenType> s_registers;

    /** @brief Set of all recognized 8086 instruction mnemonics */
    static const std::unordered_set<std::string> s_instructions;
};

} // namespace e2asm
