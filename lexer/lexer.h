#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "token.h"
#include "source_location.h"

namespace e2asm {

class Lexer {
public:
    explicit Lexer(std::string_view source, std::string filename = "<input>");

    std::vector<Token> tokenize();

private:
    Token nextToken();
    Token scanNumber();
    Token scanIdentifier();
    Token scanString();
    Token scanCharacter();

    bool isAtEnd() const;
    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);
    void skipWhitespace();
    void skipLineComment();

    SourceLocation currentLocation() const;
    void advanceLocation(char c);

    bool isDigit(char c) const;
    bool isHexDigit(char c) const;
    bool isAlpha(char c) const;
    bool isAlphaNumeric(char c) const;

    TokenType identifierType(const std::string& text) const;
    TokenType registerType(const std::string& text) const;

    std::string_view m_source;
    std::string m_filename;
    size_t m_current;
    size_t m_line;
    size_t m_column;

    static const std::unordered_map<std::string, TokenType> s_keywords;
    static const std::unordered_map<std::string, TokenType> s_registers;
    static const std::unordered_set<std::string> s_instructions;
};

} // namespace e2asm
