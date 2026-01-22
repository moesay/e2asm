#pragma once

#include <string>
#include <variant>
#include <cstdint>
#include "source_location.h"

namespace e2asm {

enum class TokenType {
    // Literals
    IDENTIFIER,        // label names, symbols
    NUMBER,           // 42, 0x2A, 0b101010, 52o
    STRING,           // "hello world"
    CHARACTER,        // 'A'

    // Registers (8-bit)
    REG8_AL,  REG8_CL,  REG8_DL,  REG8_BL,
    REG8_AH,  REG8_CH,  REG8_DH,  REG8_BH,

    // Registers (16-bit)
    REG16_AX, REG16_CX, REG16_DX, REG16_BX,
    REG16_SP, REG16_BP, REG16_SI, REG16_DI,

    // Segment Registers
    SEGREG_ES, SEGREG_CS, SEGREG_SS, SEGREG_DS,

    // Instructions (will be identified by lookup)
    INSTRUCTION,      // MOV, ADD, JMP, etc...

    // Directives
    DIR_DB, DIR_DW, DIR_DD, DIR_DQ, DIR_DT,    // Data directives
    DIR_EQU,                                    // Constants
    DIR_SEGMENT, DIR_SECTION, DIR_ENDS,         // Segments (SEGMENT and SECTION are synonyms)
    DIR_ORG,                                    // Origin
    DIR_RESB, DIR_RESW, DIR_RESD, DIR_RESQ, DIR_REST,  // Reserve space
    DIR_TIMES,                                  // Repeat

    // Preprocessor directives
    PREP_DEFINE,      // %define
    PREP_MACRO,       // %macro
    PREP_ENDMACRO,    // %endmacro
    PREP_IF,          // %if
    PREP_ELIF,        // %elif
    PREP_ELSE,        // %else
    PREP_ENDIF,       // %endif
    PREP_IFDEF,       // %ifdef
    PREP_IFNDEF,      // %ifndef
    PREP_INCLUDE,     // %include

    // Operators
    PLUS,             // +
    MINUS,            // -
    STAR,             // *
    SLASH,            // /
    PERCENT,          // %
    SHL_OP,           // <<
    SHR_OP,           // >>
    AND_OP,           // &
    OR_OP,            // |
    XOR_OP,           // ^
    TILDE,            // ~

    // Punctuation
    COMMA,            // ,
    COLON,            // :
    LBRACKET,         // [
    RBRACKET,         // ]
    LPAREN,           // (
    RPAREN,           // )
    DOT,              // .
    DOLLAR,           // $ (current position marker)
    DOUBLE_DOLLAR,    // $$ (segment start marker)

    // Size specifiers
    BYTE_PTR,         // BYTE, BPTR
    WORD_PTR,         // WORD, WPTR
    DWORD_PTR,        // DWORD, DPTR

    // Jump modifiers
    SHORT_KW,         // SHORT
    NEAR_KW,          // NEAR
    FAR_KW,           // FAR

    // Special
    NEWLINE,
    END_OF_FILE,
    INVALID
};

// A token can be a number, a string or just nothing
using TokenValue = std::variant<std::monostate, int64_t, double, std::string>;

struct Token {
    TokenType type;
    std::string lexeme;        // Original text from source
    TokenValue value;          // Parsed value (for numbers/strings)
    SourceLocation location;   // Where in source code

    Token() : type(TokenType::INVALID), lexeme(""), value(std::monostate{}), location() {}

    Token(TokenType t, std::string lex, SourceLocation loc)
        : type(t), lexeme(std::move(lex)), value(std::monostate{}), location(loc) {}

    Token(TokenType t, std::string lex, TokenValue val, SourceLocation loc)
        : type(t), lexeme(std::move(lex)), value(std::move(val)), location(loc) {}

    // Get the numeric value if the token is a nu,ber
    int64_t getNumber() const {
        if (std::holds_alternative<int64_t>(value)) {
            return std::get<int64_t>(value);
        }
        return 0;
    }

    // Get the numeric value if the token is a string
    std::string getString() const {
        if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        }
        return lexeme;
    }

    bool isRegister() const {
        return (type >= TokenType::REG8_AL && type <= TokenType::REG8_BH) ||
               (type >= TokenType::REG16_AX && type <= TokenType::REG16_DI);
    }

    bool isReg8() const {
        return type >= TokenType::REG8_AL && type <= TokenType::REG8_BH;
    }

    bool isReg16() const {
        return type >= TokenType::REG16_AX && type <= TokenType::REG16_DI;
    }

    bool isSegReg() const {
        return type >= TokenType::SEGREG_ES && type <= TokenType::SEGREG_DS;
    }

    std::string format() const;
};

} // namespace e2asm
