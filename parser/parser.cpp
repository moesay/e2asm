#include "parser.h"
#include "expression_parser.h"
#include <algorithm>

namespace e2asm {

Parser::Parser(std::vector<Token> tokens)
    : m_tokens(std::move(tokens))
    , m_current(0)
{
    // Discard all newline tokens and forget about them
    m_tokens.erase(
        std::remove_if(m_tokens.begin(), m_tokens.end(),
            [](const Token& t) { return t.type == TokenType::NEWLINE; }),
        m_tokens.end()
    );
}

std::unique_ptr<Program> Parser::parse() {
    auto program = std::make_unique<Program>();

    while (!isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) {
            program->statements.push_back(std::move(stmt));
        }
    }

    return program;
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
  /**
    To support consecutive labels, we will start with parseLabel() then the parse() loop
    will call us again
  */

    // Check for label (identifier followed by colon)
    if (check(TokenType::IDENTIFIER) && peekNext().type == TokenType::COLON) {
        return parseLabel();
    }

    // Check for EQU directive (identifier followed by EQU)
    if (check(TokenType::IDENTIFIER) && peekNext().type == TokenType::DIR_EQU) {
        return parseEQUDirective();
    }

    // Check for label before data/res directive
    // NASM style support: "label db value" without colon
    if (check(TokenType::IDENTIFIER)) {
        TokenType next = peekNext().type;
        if (next == TokenType::DIR_DB || next == TokenType::DIR_DW ||
            next == TokenType::DIR_DD || next == TokenType::DIR_DQ ||
            next == TokenType::DIR_DT || next == TokenType::DIR_RESB ||
            next == TokenType::DIR_RESW || next == TokenType::DIR_RESD ||
            next == TokenType::DIR_RESQ || next == TokenType::DIR_REST) {
            // Parse as label without colon
            Token label_token = advance();
            return std::make_unique<Label>(label_token.lexeme, label_token.location);
        }
    }

    // Check for data directives
    if (check(TokenType::DIR_DB) || check(TokenType::DIR_DW) ||
        check(TokenType::DIR_DD) || check(TokenType::DIR_DQ) ||
        check(TokenType::DIR_DT)) {
        return parseDataDirective();
    }

    // Check for ORG directive
    if (check(TokenType::DIR_ORG)) {
        return parseORGDirective();
    }

    // Check for SEGMENT or SECTION directive
    if (check(TokenType::DIR_SEGMENT) || check(TokenType::DIR_SECTION)) {
        return parseSEGMENTDirective();
    }

    // Check for ENDS directive
    if (check(TokenType::DIR_ENDS)) {
        return parseENDSDirective();
    }

    // Check for RESx directives
    if (check(TokenType::DIR_RESB) || check(TokenType::DIR_RESW) ||
        check(TokenType::DIR_RESD) || check(TokenType::DIR_RESQ) ||
        check(TokenType::DIR_REST)) {
        return parseRESDirective();
    }

    // Check for TIMES directive
    if (check(TokenType::DIR_TIMES)) {
        return parseTIMESDirective();
    }

    // Check for instruction
    if (check(TokenType::INSTRUCTION)) {
        return parseInstruction();
    }

    // Unknown statement
    error("Expected instruction, label, or directive");
    advance();  // Go about the life
    return nullptr;
}

std::unique_ptr<Instruction> Parser::parseInstruction() {
    Token instr_token = consume(TokenType::INSTRUCTION, "Expected instruction");
    auto instr = std::make_unique<Instruction>(instr_token.lexeme, instr_token.location);

    // Parse operands (comma-separated)
    // BUT: Don't parse an IDENTIFIER as an operand if it's followed by a colon or data directive
    // (that would be a label, not an operand)
    // TODO: Think about supporting MOV DEST, Label: to enhance the coding experience

    if (!isAtEnd() && isOperandStart(peek().type)) {
        // Check for label: "identifier:" or "identifier DB/DW/etc"
        // This should NOT be parsed as an operand (so far)
        if (peek().type == TokenType::IDENTIFIER) {
            TokenType next = peekNext().type;
            if (next == TokenType::COLON ||
                next == TokenType::DIR_DB || next == TokenType::DIR_DW ||
                next == TokenType::DIR_DD || next == TokenType::DIR_DQ ||
                next == TokenType::DIR_DT) {
                // Don't parse this as an operand - it's a label
                return instr;
            }
        }

        // First operand
        auto op = parseOperand(instr_token.lexeme);
        if (op) {
            instr->operands.push_back(std::move(op));
        }

        // Additional operands after commas
        while (match(TokenType::COMMA)) {
            auto next_op = parseOperand(instr_token.lexeme);
            if (next_op) {
                instr->operands.push_back(std::move(next_op));
            }
        }
    }

    return instr;
}

std::unique_ptr<Label> Parser::parseLabel() {
    Token label_token = consume(TokenType::IDENTIFIER, "Expected label name");
    consume(TokenType::COLON, "Expected ':' after label");

    return std::make_unique<Label>(label_token.lexeme, label_token.location);
}

std::unique_ptr<Operand> Parser::parseOperand(const std::string& mnemonic) {
    // Check for size specifier (BYTE PTR, WORD PTR)
    uint8_t size_hint = 0;
    if (match(TokenType::BYTE_PTR)) {
        size_hint = 8;
        // Optional PTR keyword (already consumed by BYTE_PTR token)
    } else if (match(TokenType::WORD_PTR)) {
        size_hint = 16;
    }

    // Check for segment override prefix: ES:, CS:, SS:, DS:
    std::optional<std::string> segment_override;
    if (peek().isSegReg() && peekNext().type == TokenType::COLON) {
        Token seg_token = advance();  // consume segment register
        advance();  // consume colon
        segment_override = seg_token.lexeme;
    }

    // Memory operand [...]
    if (check(TokenType::LBRACKET)) {
        return parseMemory(segment_override, size_hint);
    }

    // Register operand
    if (isRegisterToken(peek().type)) {
        return parseRegister();
    }

    // Immediate value (number or character)
    // Handle optional unary minus/plus
    if (check(TokenType::NUMBER) || check(TokenType::CHARACTER) ||
        check(TokenType::MINUS) || check(TokenType::PLUS)) {
        return parseImmediate(size_hint);
    }

    // Check for jump distance specifier (SHORT, NEAR, FAR)
    // Determine default jump type based on instruction mnemonic
    std::string mnem_upper = mnemonic;
    for (char& c : mnem_upper) c = std::toupper(c);

    // Conditional jumps on 8086 only support SHORT (8-bit relative)
    // Unconditional JMP and CALL default to NEAR for conservative estimation
    // IMPORTANT NOTE: backward jumps are optimized to SHORT during semantic analysis
    LabelRef::JumpType jump_type = LabelRef::JumpType::NEAR;

    // TODO: Do something about this mess but for now, lets get it to work
    if (mnem_upper == "JO" || mnem_upper == "JNO" || mnem_upper == "JB" || mnem_upper == "JC" ||
        mnem_upper == "JNAE" || mnem_upper == "JNB" || mnem_upper == "JAE" || mnem_upper == "JNC" ||
        mnem_upper == "JE" || mnem_upper == "JZ" || mnem_upper == "JNE" || mnem_upper == "JNZ" ||
        mnem_upper == "JBE" || mnem_upper == "JNA" || mnem_upper == "JNBE" || mnem_upper == "JA" ||
        mnem_upper == "JS" || mnem_upper == "JNS" || mnem_upper == "JP" || mnem_upper == "JPE" ||
        mnem_upper == "JNP" || mnem_upper == "JPO" || mnem_upper == "JL" || mnem_upper == "JNGE" ||
        mnem_upper == "JNL" || mnem_upper == "JGE" || mnem_upper == "JLE" || mnem_upper == "JNG" ||
        mnem_upper == "JNLE" || mnem_upper == "JG" || mnem_upper == "LOOP" || mnem_upper == "LOOPE" ||
        mnem_upper == "LOOPZ" || mnem_upper == "LOOPNE" || mnem_upper == "LOOPNZ" || mnem_upper == "JCXZ") {
        jump_type = LabelRef::JumpType::SHORT;  // Conditional jumps are always SHORT
    }

    if (match(TokenType::SHORT_KW)) {
        jump_type = LabelRef::JumpType::SHORT;
    } else if (match(TokenType::NEAR_KW)) {
        jump_type = LabelRef::JumpType::NEAR;
    } else if (match(TokenType::FAR_KW)) {
        jump_type = LabelRef::JumpType::FAR;
    }

    // Label reference or expression (for jumps or immediate values like MOV AX, .data or ADD DI, VAR1(-/+)VAR2)
    if (check(TokenType::IDENTIFIER)) {
        Token label_token = advance();
        std::string expression = label_token.lexeme;

        // Check if followed by arithmetic operators
        while (check(TokenType::PLUS) || check(TokenType::MINUS) ||
               check(TokenType::STAR) || check(TokenType::SLASH)) {
            Token op = advance();
            expression += " " + op.lexeme + " ";

            // Expect identifier or number after operator
            if (check(TokenType::IDENTIFIER)) {
                Token operand = advance();
                expression += operand.lexeme;
            } else if (check(TokenType::NUMBER)) {
                Token operand = advance();
                expression += operand.lexeme;
            } else {
                error("Expected identifier or number after operator");
                break;
            }
        }

        // Check if this instruction is a jump/call/loop - use LabelRef
        if (mnem_upper == "JMP" || mnem_upper == "CALL" || mnem_upper == "JO" || mnem_upper == "JNO" ||
            mnem_upper == "JB" || mnem_upper == "JC" || mnem_upper == "JNAE" || mnem_upper == "JNB" ||
            mnem_upper == "JAE" || mnem_upper == "JNC" || mnem_upper == "JE" || mnem_upper == "JZ" ||
            mnem_upper == "JNE" || mnem_upper == "JNZ" || mnem_upper == "JBE" || mnem_upper == "JNA" ||
            mnem_upper == "JNBE" || mnem_upper == "JA" || mnem_upper == "JS" || mnem_upper == "JNS" ||
            mnem_upper == "JP" || mnem_upper == "JPE" || mnem_upper == "JNP" || mnem_upper == "JPO" ||
            mnem_upper == "JL" || mnem_upper == "JNGE" || mnem_upper == "JNL" || mnem_upper == "JGE" ||
            mnem_upper == "JLE" || mnem_upper == "JNG" || mnem_upper == "JNLE" || mnem_upper == "JG" ||
            mnem_upper == "LOOP" || mnem_upper == "LOOPE" || mnem_upper == "LOOPZ" ||
            mnem_upper == "LOOPNE" || mnem_upper == "LOOPNZ" || mnem_upper == "JCXZ") {
            return std::make_unique<LabelRef>(expression, label_token.location, jump_type);
        }

        // Otherwise, treat as immediate operand with label/expression ref
        return std::make_unique<ImmediateOperand>(expression, label_token.location, size_hint);
    }

    error("Expected operand (register, immediate, or memory address)");
    return nullptr;
}

std::unique_ptr<RegisterOperand> Parser::parseRegister() {
    Token reg_token = advance();

    uint8_t code = getRegisterCode(reg_token.type);
    uint8_t size = getRegisterSize(reg_token.type);
    bool is_seg = reg_token.isSegReg();

    return std::make_unique<RegisterOperand>(
        reg_token.lexeme, size, code, is_seg, reg_token.location
    );
}

std::unique_ptr<ImmediateOperand> Parser::parseImmediate(uint8_t size_hint) {
    SourceLocation loc = peek().location;

    // Collect tokens that form an expression
    // Valid expression tokens: NUMBER, CHARACTER, IDENTIFIER, PLUS, MINUS, STAR, SLASH, LPAREN, RPAREN
    // IMPORTANT: IDENTIFIER should only be consumed after an operator (to allow label+offset),
    // not at the start or after a value (to avoid consuming next line's label)
    std::string expr;
    bool has_identifier = false;
    int paren_depth = 0;
    bool last_was_operator = true;  // Start true to allow identifier at beginning

    while (!isAtEnd()) {
        TokenType type = peek().type;

        // Check for operators first
        if (type == TokenType::PLUS || type == TokenType::MINUS ||
            type == TokenType::STAR || type == TokenType::SLASH) {
            Token t = advance();
            expr += t.lexeme;
            last_was_operator = true;
        }
        // Parentheses
        else if (type == TokenType::LPAREN) {
            Token t = advance();
            paren_depth++;
            expr += t.lexeme;
            last_was_operator = true;  // After '(' we expect a value
        }
        else if (type == TokenType::RPAREN) {
            Token t = advance();
            paren_depth--;
            expr += t.lexeme;
            last_was_operator = false;  // After ')' we have a complete subexpression
        }
        // Numbers and characters (values)
        else if (type == TokenType::NUMBER) {
            Token t = advance();
            expr += std::to_string(t.getNumber());
            last_was_operator = false;
        }
        else if (type == TokenType::CHARACTER) {
            Token t = advance();
            std::string char_str = t.getString();
            if (!char_str.empty()) {
                expr += std::to_string(static_cast<int>(char_str[0]));
            }
            last_was_operator = false;
        }
        // Identifiers - only consume after an operator or at the start
        else if (type == TokenType::IDENTIFIER && last_was_operator) {
            Token t = advance();
            has_identifier = true;
            expr += t.lexeme;
            last_was_operator = false;
        }
        else {
            // Not part of expression, stop
            break;
        }
    }

    if (expr.empty()) {
        error("Expected immediate value or expression");
        return nullptr;
    }

    if (has_identifier) {
        // Contains labels - store as expression for later resolution during encoding
        return std::make_unique<ImmediateOperand>(expr, loc, size_hint);
    } else {
        // Pure numeric expression - evaluate now
        auto result = ExpressionParser::evaluate(expr);
        if (!result) {
            error("Invalid expression: " + expr);
            return nullptr;
        }
        return std::make_unique<ImmediateOperand>(*result, loc, size_hint);
    }
}

std::unique_ptr<MemoryOperand> Parser::parseMemory(const std::optional<std::string>& segment_override, uint8_t size_hint) {
    SourceLocation loc = peek().location;
    consume(TokenType::LBRACKET, "Expected '['");

    // Collect address expression
    std::string address_expr;
    while (!check(TokenType::RBRACKET) && !isAtEnd()) {
        Token t = advance();
        if (!address_expr.empty() && t.type != TokenType::PLUS &&
            t.type != TokenType::MINUS && t.type != TokenType::STAR &&
            t.type != TokenType::SLASH) {
            // Add space between tokens for readability
            if (address_expr.back() != '+' && address_expr.back() != '-' &&
                address_expr.back() != '*' && address_expr.back() != '/') {
                address_expr += " ";
            }
        }
        address_expr += t.lexeme;
    }

    consume(TokenType::RBRACKET, "Expected ']'");

    auto mem_op = std::make_unique<MemoryOperand>(address_expr, loc, size_hint);

    // Store segment override if provided (from outside brackets like "ES:[DI]")
    std::optional<std::string> final_segment_override = segment_override;

    // Check for segment override inside the address expression (like "[ES:DI]")
    // This is common in NASM syntax
    size_t colon_pos = address_expr.find(':');
    if (colon_pos != std::string::npos) {
        // Extract potential segment register prefix
        std::string prefix = address_expr.substr(0, colon_pos);
        size_t start = prefix.find_first_not_of(" \t");
        size_t end = prefix.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            prefix = prefix.substr(start, end - start + 1);
        }

        std::string prefix_upper = prefix;
        for (char& c : prefix_upper) c = std::toupper(c);

        if (prefix_upper == "ES" || prefix_upper == "CS" ||
            prefix_upper == "SS" || prefix_upper == "DS") {
            // If its a valid segment prefix, extract segment override and remove it from address expression
            final_segment_override = prefix_upper;
            // Get the part after the colon
            address_expr = address_expr.substr(colon_pos + 1);
            start = address_expr.find_first_not_of(" \t");
            if (start != std::string::npos) {
                address_expr = address_expr.substr(start);
            }
        }
    }

    mem_op->segment_override = final_segment_override;
    mem_op->address_expr = address_expr;

    // Parse the address expression
    auto parsed = ExpressionParser::parseAddress(address_expr);
    if (parsed) {
        // Check if it's a direct address (no registers, just a number)
        if (parsed->registers.empty() && parsed->has_displacement) {
            mem_op->is_direct_address = true;
            mem_op->direct_address_value = static_cast<uint16_t>(parsed->displacement);
        } else {
            // Store parsed address
            mem_op->parsed_address = std::make_unique<AddressExpression>(*parsed);
        }
    } else {
        // Failed to parse
        // Note: This will be caught during the code generation
    }

    return mem_op;
}

Token Parser::peek() const {
    return m_tokens[m_current];
}

Token Parser::peekNext() const {
    if (m_current + 1 < m_tokens.size()) {
        return m_tokens[m_current + 1];
    }
    return m_tokens[m_current];  // Return current if no next
}

Token Parser::advance() {
    if (!isAtEnd()) {
        m_current++;
    }
    return m_tokens[m_current - 1];
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }

    error(message);
    return peek();
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::END_OF_FILE;
}

bool Parser::isOperandStart(TokenType type) const {
    return isRegisterToken(type) ||
      type == TokenType::NUMBER ||
      type == TokenType::CHARACTER ||
      type == TokenType::IDENTIFIER ||
      type == TokenType::LBRACKET ||
      type == TokenType::BYTE_PTR ||
      type == TokenType::WORD_PTR ||
      type == TokenType::MINUS ||
      type == TokenType::PLUS ||
      type == TokenType::SHORT_KW ||
      type == TokenType::NEAR_KW ||
      type == TokenType::FAR_KW;
}

void Parser::error(const std::string& message) {
    m_error_reporter.error(message, peek().location);
}

void Parser::synchronize() {
    advance();

    // Skip until we find a likely statement boundary
    while (!isAtEnd()) {
        if (check(TokenType::INSTRUCTION) || check(TokenType::IDENTIFIER)) {
            return;
        }
        advance();
    }
}

uint8_t Parser::getRegisterCode(TokenType reg) const {
    switch (reg) {
        // 8-bit registers
        case TokenType::REG8_AL: return 0;
        case TokenType::REG8_CL: return 1;
        case TokenType::REG8_DL: return 2;
        case TokenType::REG8_BL: return 3;
        case TokenType::REG8_AH: return 4;
        case TokenType::REG8_CH: return 5;
        case TokenType::REG8_DH: return 6;
        case TokenType::REG8_BH: return 7;

        // 16-bit registers
        case TokenType::REG16_AX: return 0;
        case TokenType::REG16_CX: return 1;
        case TokenType::REG16_DX: return 2;
        case TokenType::REG16_BX: return 3;
        case TokenType::REG16_SP: return 4;
        case TokenType::REG16_BP: return 5;
        case TokenType::REG16_SI: return 6;
        case TokenType::REG16_DI: return 7;

        // Segment registers
        case TokenType::SEGREG_ES: return 0;
        case TokenType::SEGREG_CS: return 1;
        case TokenType::SEGREG_SS: return 2;
        case TokenType::SEGREG_DS: return 3;

        default: return 0;
    }
}

uint8_t Parser::getRegisterSize(TokenType reg) const {
    if (reg >= TokenType::REG8_AL && reg <= TokenType::REG8_BH) {
        return 8;
    } else if (reg >= TokenType::REG16_AX && reg <= TokenType::REG16_DI) {
        return 16;
    } else if (reg >= TokenType::SEGREG_ES && reg <= TokenType::SEGREG_DS) {
        return 16;  // Segment registers are 16-bit
    }
    return 0;
}

bool Parser::isRegisterToken(TokenType type) const {
    return (type >= TokenType::REG8_AL && type <= TokenType::REG8_BH) ||
           (type >= TokenType::REG16_AX && type <= TokenType::REG16_DI) ||
           (type >= TokenType::SEGREG_ES && type <= TokenType::SEGREG_DS);
}

std::unique_ptr<DataDirective> Parser::parseDataDirective() {
    Token directive_token = advance();

    DataDirective::Size size;
    switch (directive_token.type) {
        case TokenType::DIR_DB: size = DataDirective::Size::BYTE; break;
        case TokenType::DIR_DW: size = DataDirective::Size::WORD; break;
        case TokenType::DIR_DD: size = DataDirective::Size::DWORD; break;
        case TokenType::DIR_DQ: size = DataDirective::Size::QWORD; break;
        case TokenType::DIR_DT: size = DataDirective::Size::TBYTE; break;
        default:
            error("Invalid data directive");
            return nullptr;
    }

    auto directive = std::make_unique<DataDirective>(size, directive_token.location);

    // Parse comma-separated values
    do {
        if (check(TokenType::STRING)) {
            // String literal
            Token str_token = advance();
            directive->values.emplace_back(str_token.lexeme, DataValue::Type::STRING);
        }
        else if (check(TokenType::CHARACTER)) {
            // Character literal
            Token char_token = advance();
            directive->values.emplace_back(char_token.lexeme, DataValue::Type::CHARACTER);
        }
        else if (check(TokenType::NUMBER)) {
            // Numeric value
            Token num_token = advance();
            directive->values.emplace_back(num_token.getNumber());
        }
        else if (check(TokenType::IDENTIFIER)) {
            // Symbol reference (EQU constant or label) - resolved during semantic analysis
            Token id_token = advance();
            directive->values.emplace_back(id_token.lexeme, DataValue::Type::SYMBOL);
        }
        else {
            error("Expected number, string, character literal, or symbol");
            return directive;
        }

        // Check for comma (more values)
    } while (match(TokenType::COMMA));

    return directive;
}

std::unique_ptr<EQUDirective> Parser::parseEQUDirective() {
    Token name_token = consume(TokenType::IDENTIFIER, "Expected constant name");
    consume(TokenType::DIR_EQU, "Expected EQU");

    Token value_token = consume(TokenType::NUMBER, "Expected numeric value");

    return std::make_unique<EQUDirective>(
        name_token.lexeme,
        value_token.getNumber(),
        name_token.location
    );
}

std::unique_ptr<ORGDirective> Parser::parseORGDirective() {
    Token org_token = consume(TokenType::DIR_ORG, "Expected ORG");

    // Parse address (could be a number or expression)
    // For now, just parse as a number
    Token addr_token = consume(TokenType::NUMBER, "Expected address after ORG");
    int64_t address = addr_token.getNumber();

    return std::make_unique<ORGDirective>(address, org_token.location);
}

std::unique_ptr<SEGMENTDirective> Parser::parseSEGMENTDirective() {
    // Accept either SEGMENT or SECTION because NASM supports both
    // For flat binaries, they are the same so just add both for convenience.
    Token seg_token = peek();
    if (seg_token.type != TokenType::DIR_SEGMENT && seg_token.type != TokenType::DIR_SECTION) {
        error("Expected SEGMENT or SECTION");
        return nullptr;
    }
    advance();  // Consume SEGMENT or SECTION token

    Token name_token = consume(TokenType::IDENTIFIER, "Expected segment name");

    return std::make_unique<SEGMENTDirective>(name_token.lexeme, seg_token.location);
}

std::unique_ptr<ENDSDirective> Parser::parseENDSDirective() {
    // ENDS can be: "segment_name ENDS" or just "ENDS"
    Token ends_token = consume(TokenType::DIR_ENDS, "Expected ENDS");

    /**
      For now, ENDS is optional for a segment name, but
      This is not NASM standard syntax,
      TODO: mimic NASM
    */
    std::string name = "";  // Empty means close current segment

    return std::make_unique<ENDSDirective>(name, ends_token.location);
}

std::unique_ptr<RESDirective> Parser::parseRESDirective() {
    Token directive_token = advance();

    // Determine size based on directive type
    RESDirective::Size size;
    switch (directive_token.type) {
        case TokenType::DIR_RESB: size = RESDirective::Size::BYTE; break;
        case TokenType::DIR_RESW: size = RESDirective::Size::WORD; break;
        case TokenType::DIR_RESD: size = RESDirective::Size::DWORD; break;
        case TokenType::DIR_RESQ: size = RESDirective::Size::QWORD; break;
        case TokenType::DIR_REST: size = RESDirective::Size::TBYTE; break;
        default:
            error("Invalid RES directive");
            return nullptr;
    }

    // Parse count
    Token count_token = consume(TokenType::NUMBER, "Expected count after RES directive");
    int64_t count = count_token.getNumber();

    return std::make_unique<RESDirective>(size, count, directive_token.location);
}

std::unique_ptr<TIMESDirective> Parser::parseTIMESDirective() {
    Token times_token = consume(TokenType::DIR_TIMES, "Expected TIMES");

    // Parse the count - can be a number or an identifier (EQU constant)
    int64_t count = -1;  // -1 indicates unresolved
    std::string count_expr;

    if (check(TokenType::NUMBER)) {
        Token count_token = advance();
        count = count_token.getNumber();
        count_expr = count_token.lexeme;
    }
    else if (check(TokenType::IDENTIFIER)) {
        // Symbol reference - will be resolved during semantic analysis
        Token id_token = advance();
        count_expr = id_token.lexeme;
        // count remains -1 to indicate it needs resolution
    }
    else {
        error("Expected count (number or constant) after TIMES");
        return nullptr;
    }

    // Parse the repeated statement
    auto repeated = parseStatement();
    if (!repeated) {
        error("Expected statement after TIMES directive");
        return nullptr;
    }

    auto times_node = std::make_unique<TIMESDirective>(count, count_expr, times_token.location);
    times_node->repeated_node = std::move(repeated);

    return times_node;
}

} // namespace e2asm
