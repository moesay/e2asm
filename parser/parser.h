#pragma once

#include <vector>
#include <memory>
#include "ast.h"
#include "../lexer/token.h"
#include "../core/error.h"

namespace e2asm {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::unique_ptr<Program> parse();

    const std::vector<Error>& errors() const { return m_error_reporter.getErrors(); }

    bool hasErrors() const { return m_error_reporter.hasErrors(); }

private:
    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<Instruction> parseInstruction();
    std::unique_ptr<Label> parseLabel();
    std::unique_ptr<DataDirective> parseDataDirective();
    std::unique_ptr<EQUDirective> parseEQUDirective();
    std::unique_ptr<ORGDirective> parseORGDirective();
    std::unique_ptr<SEGMENTDirective> parseSEGMENTDirective();
    std::unique_ptr<ENDSDirective> parseENDSDirective();
    std::unique_ptr<RESDirective> parseRESDirective();
    std::unique_ptr<TIMESDirective> parseTIMESDirective();

    std::unique_ptr<Operand> parseOperand(const std::string& mnemonic = "");
    std::unique_ptr<RegisterOperand> parseRegister();
    std::unique_ptr<ImmediateOperand> parseImmediate();
    std::unique_ptr<MemoryOperand> parseMemory(const std::optional<std::string>& segment_override = std::nullopt, uint8_t size_hint = 0);

    Token peek() const;
    Token peekNext() const;
    Token advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    bool isOperandStart(TokenType type) const;
    Token consume(TokenType type, const std::string& message);
    bool isAtEnd() const;

    void error(const std::string& message);
    void synchronize();  // Error recovery

    uint8_t getRegisterCode(TokenType reg) const;
    uint8_t getRegisterSize(TokenType reg) const;
    bool isRegisterToken(TokenType type) const;

    std::vector<Token> m_tokens;
    size_t m_current;
    ErrorReporter m_error_reporter;
};

} // namespace e2asm
