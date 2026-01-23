/**
 * @file parser.h
 * @brief Syntax analyzer that builds an AST from tokens
 *
 * The parser is the second compilation phase. It consumes the token stream
 * from the lexer and constructs an Abstract Syntax Tree representing the
 * program's structure. Validates syntax and reports errors with precise locations.
 */

#pragma once

#include <vector>
#include <memory>
#include "ast.h"
#include "../lexer/token.h"
#include "../core/error.h"

namespace e2asm {

/**
 * @brief Converts a token stream into an Abstract Syntax Tree
 *
 * The parser recognizes the grammar of 8086 assembly: instructions with operands,
 * labels, data definitions, and assembler directives. It builds a tree structure
 * that represents the source's logical organization.
 *
 * Uses recursive descent parsing with one token lookahead. Implements error
 * recovery via synchronization to report multiple syntax errors in one pass.
 *
 * Parsing doesn't resolve symbols or generate code - it just validates structure
 * and builds the tree. Semantic analysis and code generation happen later.
 */
class Parser {
public:
    /**
     * @brief Constructs a parser for the given token stream
     * @param tokens Complete token sequence from the lexer (must include END_OF_FILE)
     */
    explicit Parser(std::vector<Token> tokens);

    /**
     * @brief Parses the token stream into an AST
     * @return Program node containing all statements, or partial tree if errors occurred
     *
     * Continues parsing after errors to find multiple issues. Check hasErrors()
     * to see if the tree is valid. Invalid trees shouldn't be passed to later phases.
     */
    std::unique_ptr<Program> parse();

    /**
     * @brief Gets all syntax errors encountered
     * @return Vector of errors with source locations
     */
    const std::vector<Error>& errors() const { return m_error_reporter.getErrors(); }

    /**
     * @brief Checks if any errors were encountered
     * @return true if parsing failed due to syntax errors
     */
    bool hasErrors() const { return m_error_reporter.hasErrors(); }

private:
    /** @brief Parses any top-level statement (instruction, label, directive) */
    std::unique_ptr<ASTNode> parseStatement();

    /** @brief Parses an instruction with its operands */
    std::unique_ptr<Instruction> parseInstruction();

    /** @brief Parses a label definition (name followed by colon) */
    std::unique_ptr<Label> parseLabel();

    /** @brief Parses DB/DW/DD/DQ/DT data definition */
    std::unique_ptr<DataDirective> parseDataDirective();

    /** @brief Parses name EQU value constant definition */
    std::unique_ptr<EQUDirective> parseEQUDirective();

    /** @brief Parses ORG address directive */
    std::unique_ptr<ORGDirective> parseORGDirective();

    /** @brief Parses SEGMENT/SECTION name directive */
    std::unique_ptr<SEGMENTDirective> parseSEGMENTDirective();

    /** @brief Parses name ENDS directive */
    std::unique_ptr<ENDSDirective> parseENDSDirective();

    /** @brief Parses RESB/RESW/RESD/RESQ/REST space reservation */
    std::unique_ptr<RESDirective> parseRESDirective();

    /** @brief Parses TIMES count directive/instruction repetition */
    std::unique_ptr<TIMESDirective> parseTIMESDirective();

    /** @brief Parses an instruction operand (register, immediate, memory, label) */
    std::unique_ptr<Operand> parseOperand(const std::string& mnemonic = "");

    /** @brief Parses a register operand (AX, BL, etc.) */
    std::unique_ptr<RegisterOperand> parseRegister();

    /** @brief Parses an immediate value or symbol */
    std::unique_ptr<ImmediateOperand> parseImmediate();

    /** @brief Parses a memory operand [expression] */
    std::unique_ptr<MemoryOperand> parseMemory(const std::optional<std::string>& segment_override = std::nullopt, uint8_t size_hint = 0);

    /** @brief Returns current token without consuming it */
    Token peek() const;

    /** @brief Returns next token without consuming current */
    Token peekNext() const;

    /** @brief Consumes and returns current token */
    Token advance();

    /** @brief Consumes token if it matches type, returns true if matched */
    bool match(TokenType type);

    /** @brief Checks if current token matches type without consuming */
    bool check(TokenType type) const;

    /** @brief Checks if token could start an operand */
    bool isOperandStart(TokenType type) const;

    /** @brief Consumes token of expected type or reports error */
    Token consume(TokenType type, const std::string& message);

    /** @brief Checks if we've consumed all tokens */
    bool isAtEnd() const;

    /** @brief Reports a syntax error at current position */
    void error(const std::string& message);

    /** @brief Skips tokens until likely start of next statement (error recovery) */
    void synchronize();

    /** @brief Gets 3-bit register encoding for ModR/M byte */
    uint8_t getRegisterCode(TokenType reg) const;

    /** @brief Gets register size (8 or 16 bits) */
    uint8_t getRegisterSize(TokenType reg) const;

    /** @brief Checks if token type is any register */
    bool isRegisterToken(TokenType type) const;

    std::vector<Token> m_tokens;      ///< Token stream to parse
    size_t m_current;                 ///< Index of next token to consume
    ErrorReporter m_error_reporter;   ///< Collects syntax errors
};

} // namespace e2asm
