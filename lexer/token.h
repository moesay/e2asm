/**
 * @file token.h
 * @brief Token definitions for the lexical analyzer
 *
 * Defines all token types recognized by the E2Asm lexer, from registers and
 * instructions to operators and directives. Each token carries its type, original
 * text (lexeme), parsed value if applicable, and source location.
 */

#pragma once

#include <string>
#include <variant>
#include <cstdint>
#include "source_location.h"

namespace e2asm {

/**
 * @brief Every category of token recognized by the lexer
 *
 * The lexer scans assembly source and categorizes each meaningful sequence into
 * one of these types. Register names become REG8/REG16 tokens, numbers become
 * NUMBER tokens, and so on. This classification drives the parser's decision-making.
 */
enum class TokenType {
    // Literals
    IDENTIFIER,        // label names, symbols
    NUMBER,           // 42, 0x2A, 0b101010, 52o
    STRING,           // "hello world"
    CHARACTER,        // 'A', This token is never used in the project but It's here just in case

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
    NEWLINE,      ///< End of a logical line (statement separator)
    END_OF_FILE,  ///< Marks the end of input
    INVALID       ///< Lexical error (unrecognized character sequence)
};

/**
 * @brief Optional parsed value attached to a token
 *
 * Most tokens just have a type and lexeme. But NUMBER tokens also carry the
 * parsed numeric value, and STRING tokens carry the unescaped string content.
 * Uses std::variant to type-safely hold either nothing, an integer, or a string.
 */
using TokenValue = std::variant<std::monostate, int64_t, double, std::string>;

/**
 * @brief A single token produced by the lexer
 *
 * Tokens are the fundamental units the parser works with. Each token knows what
 * kind of element it represents (type), its original spelling in the source (lexeme),
 * any parsed value like a number or string, and exactly where it came from.
 */
struct Token {
    TokenType type;             ///< What category this token belongs to
    std::string lexeme;         ///< Exact text from the source code
    TokenValue value;           ///< Parsed value for NUMBERs and STRINGs
    SourceLocation location;    ///< Position in source where this token appears

    Token() : type(TokenType::INVALID), lexeme(""), value(std::monostate{}), location() {}

    Token(TokenType t, std::string lex, SourceLocation loc)
        : type(t), lexeme(std::move(lex)), value(std::monostate{}), location(loc) {}

    Token(TokenType t, std::string lex, TokenValue val, SourceLocation loc)
        : type(t), lexeme(std::move(lex)), value(std::move(val)), location(loc) {}

    /**
     * @brief Extracts integer value from NUMBER tokens
     * @return The parsed number, or 0 if this isn't a number token
     */
    int64_t getNumber() const {
        if (std::holds_alternative<int64_t>(value)) {
            return std::get<int64_t>(value);
        }
        return 0;
    }

    /**
     * @brief Extracts string content from STRING tokens
     * @return The parsed string, or the lexeme if this isn't a string token
     */
    std::string getString() const {
        if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        }
        return lexeme;
    }

    /**
     * @brief Checks if this is any kind of register (8-bit, 16-bit, or segment)
     * @return true for AL, AX, ES, etc.
     */
    bool isRegister() const {
        return (type >= TokenType::REG8_AL && type <= TokenType::REG8_BH) ||
               (type >= TokenType::REG16_AX && type <= TokenType::REG16_DI);
    }

    /**
     * @brief Checks if this is an 8-bit general purpose register
     * @return true for AL, CL, DL, BL, AH, CH, DH, BH
     */
    bool isReg8() const {
        return type >= TokenType::REG8_AL && type <= TokenType::REG8_BH;
    }

    /**
     * @brief Checks if this is a 16-bit general purpose register
     * @return true for AX, CX, DX, BX, SP, BP, SI, DI
     */
    bool isReg16() const {
        return type >= TokenType::REG16_AX && type <= TokenType::REG16_DI;
    }

    /**
     * @brief Checks if this is a segment register
     * @return true for ES, CS, SS, DS
     */
    bool isSegReg() const {
        return type >= TokenType::SEGREG_ES && type <= TokenType::SEGREG_DS;
    }

    /**
     * @brief Creates a human-readable representation for debugging
     * @return String describing this token's type and value
     */
    std::string format() const;
};

} // namespace e2asm
