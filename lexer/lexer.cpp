#include "lexer.h"
#include <cctype>
#include <algorithm>

namespace e2asm {

static std::string toUpper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

const std::unordered_map<std::string, TokenType> Lexer::s_keywords = {
    // Data directives
    {"DB", TokenType::DIR_DB},
    {"DW", TokenType::DIR_DW},
    {"DD", TokenType::DIR_DD},
    {"DQ", TokenType::DIR_DQ},
    {"DT", TokenType::DIR_DT},
    {"EQU", TokenType::DIR_EQU},

    // Segment directives
    {"SEGMENT", TokenType::DIR_SEGMENT},
    {"SECTION", TokenType::DIR_SECTION},
    {"ENDS", TokenType::DIR_ENDS},
    {"ORG", TokenType::DIR_ORG},

    // Reserve directives
    {"RESB", TokenType::DIR_RESB},
    {"RESW", TokenType::DIR_RESW},
    {"RESD", TokenType::DIR_RESD},
    {"RESQ", TokenType::DIR_RESQ},
    {"REST", TokenType::DIR_REST},
    {"TIMES", TokenType::DIR_TIMES},

    // Size specifiers
    {"BYTE", TokenType::BYTE_PTR},
    {"BPTR", TokenType::BYTE_PTR},
    {"WORD", TokenType::WORD_PTR},
    {"WPTR", TokenType::WORD_PTR},
    {"DWORD", TokenType::DWORD_PTR},
    {"DPTR", TokenType::DWORD_PTR},
    {"PTR", TokenType::WORD_PTR},  // Default to WORD

    // Jump modifiers
    {"SHORT", TokenType::SHORT_KW},
    {"NEAR", TokenType::NEAR_KW},
    {"FAR", TokenType::FAR_KW},
};

const std::unordered_map<std::string, TokenType> Lexer::s_registers = {
    // 8-bit registers
    {"AL", TokenType::REG8_AL}, {"CL", TokenType::REG8_CL},
    {"DL", TokenType::REG8_DL}, {"BL", TokenType::REG8_BL},
    {"AH", TokenType::REG8_AH}, {"CH", TokenType::REG8_CH},
    {"DH", TokenType::REG8_DH}, {"BH", TokenType::REG8_BH},

    // 16-bit registers
    {"AX", TokenType::REG16_AX}, {"CX", TokenType::REG16_CX},
    {"DX", TokenType::REG16_DX}, {"BX", TokenType::REG16_BX},
    {"SP", TokenType::REG16_SP}, {"BP", TokenType::REG16_BP},
    {"SI", TokenType::REG16_SI}, {"DI", TokenType::REG16_DI},

    // Segment registers
    {"ES", TokenType::SEGREG_ES}, {"CS", TokenType::SEGREG_CS},
    {"SS", TokenType::SEGREG_SS}, {"DS", TokenType::SEGREG_DS},
};

// Common 8086 instructions
const std::unordered_set<std::string> Lexer::s_instructions = {
    "MOV", "PUSH", "POP", "PUSHA", "POPA", "XCHG",
    "ADD", "SUB", "ADC", "SBB", "INC", "DEC", "NEG", "CMP",
    "MUL", "IMUL", "DIV", "IDIV",
    "AND", "OR", "XOR", "NOT", "TEST",
    "SHL", "SHR", "SAL", "SAR", "ROL", "ROR", "RCL", "RCR",
    "JMP", "JE", "JZ", "JNE", "JNZ", "JL", "JNGE", "JLE", "JNG",
    "JG", "JNLE", "JGE", "JNL", "JB", "JNAE", "JC", "JBE", "JNA",
    "JA", "JNBE", "JAE", "JNB", "JNC", "JS", "JNS", "JO", "JNO",
    "JP", "JPE", "JNP", "JPO", "JCXZ",
    "CALL", "RET", "RETF",
    "LOOP", "LOOPE", "LOOPZ", "LOOPNE", "LOOPNZ",
    "INT", "IRET", "INTO",
    "LEA", "LDS", "LES",
    "MOVS", "MOVSB", "MOVSW", "CMPS", "CMPSB", "CMPSW",
    "SCAS", "SCASB", "SCASW", "LODS", "LODSB", "LODSW",
    "STOS", "STOSB", "STOSW",
    "REP", "REPE", "REPZ", "REPNE", "REPNZ",
    "IN", "OUT",
    "HLT", "NOP", "WAIT", "ESC", "LOCK",
    "CLC", "STC", "CMC", "CLD", "STD", "CLI", "STI",
    "LAHF", "SAHF", "PUSHF", "POPF",
    "AAA", "AAS", "AAM", "AAD", "DAA", "DAS",
    "CBW", "CWD", "XLAT"
};

Lexer::Lexer(std::string_view source, std::string filename)
    : m_source(source)
    , m_filename(std::move(filename))
    , m_current(0)
    , m_line(1)
    , m_column(1)
{
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        skipWhitespace();
        if (isAtEnd()) break;

        Token token = nextToken();
        if (token.type != TokenType::INVALID) {
            tokens.push_back(token);
        }
    }

    // to avoid parser bugs, always add eof
    tokens.emplace_back(TokenType::END_OF_FILE, "", currentLocation());

    return tokens;
}

Token Lexer::nextToken() {
    char c = peek();
    SourceLocation loc = currentLocation();

    if (c == ';') {
        skipLineComment();
        return Token(TokenType::NEWLINE, "\n", loc);
    }

    if (c == '\n') {
        advance();
        return Token(TokenType::NEWLINE, "\n", loc);
    }

    if (c == '$') {
        // If there's $, First check for $$
        if (peekNext() == '$') {
            advance();  // consume first $
            advance();  // consume second $
            return Token(TokenType::DOUBLE_DOLLAR, "$$", loc);
        }
        // Check if $ is followed by a hex digit, like $FF to support nasm hex notation ;)
        if (isHexDigit(peekNext())) {
            return scanNumber();
        }
        // Standalone $ is a position marker
        advance();
        return Token(TokenType::DOLLAR, "$", loc);
    }

    if (isDigit(c)) {
        return scanNumber();
    }

    if (c == '"') {
        return scanString();
    }

    if (c == '\'') {
        return scanCharacter();
    }

    // Identifiers, keywords, registers, instructions
    if (isAlpha(c) || c == '_' || c == '.') {
        return scanIdentifier();
    }

    // Handle % separately to avoid confusing the lexer if it's not a directive
    if (isAlpha(peekNext()) && c == '%') {
      return scanIdentifier();
    }

    // Operators and punctuation
    advance();

    switch (c) {
        case '+': return Token(TokenType::PLUS, "+", loc);
        case '-': return Token(TokenType::MINUS, "-", loc);
        case '*': return Token(TokenType::STAR, "*", loc);
        case '/': return Token(TokenType::SLASH, "/", loc);
        case '%': return Token(TokenType::PERCENT, "%", loc);
        case '&': return Token(TokenType::AND_OP, "&", loc);
        case '|': return Token(TokenType::OR_OP, "|", loc);
        case '^': return Token(TokenType::XOR_OP, "^", loc);
        case '~': return Token(TokenType::TILDE, "~", loc);
        case ',': return Token(TokenType::COMMA, ",", loc);
        case ':': return Token(TokenType::COLON, ":", loc);
        case '[': return Token(TokenType::LBRACKET, "[", loc);
        case ']': return Token(TokenType::RBRACKET, "]", loc);
        case '(': return Token(TokenType::LPAREN, "(", loc);
        case ')': return Token(TokenType::RPAREN, ")", loc);
        case '.': return Token(TokenType::DOT, ".", loc);
        // TokenType::DOT should be dropped entirely because it has not meaning other that starting an identifier.

        case '<':
            if (match('<')) {
                return Token(TokenType::SHL_OP, "<<", loc);
            }
            break;

        case '>':
            if (match('>')) {
                return Token(TokenType::SHR_OP, ">>", loc);
            }
            break;
    }

    return Token(TokenType::INVALID, std::string(1, c), loc);
}

Token Lexer::scanNumber() {
    SourceLocation loc = currentLocation();
    size_t start = m_current;

    if (peek() == '$') {
        advance();
        // Hex number after $
        if (!isHexDigit(peek())) {
            // $ without hex digits is invalid
            return Token(TokenType::INVALID, "$", loc);
        }
        while (isHexDigit(peek())) {
            advance();
        }
        std::string num_str(m_source.substr(start + 1, m_current - start - 1));
        int64_t value = std::stoll(num_str, nullptr, 16);
        return Token(TokenType::NUMBER, std::string(m_source.substr(start, m_current - start)),
                    value, loc);
    }

    // Check for 0x prefix (hex)
    if (peek() == '0' && (peekNext() == 'x' || peekNext() == 'X')) {
        advance(); // '0'
        advance(); // 'x'
        while (isHexDigit(peek())) {
            advance();
        }
        std::string num_str(m_source.substr(start + 2, m_current - start - 2));
        int64_t value = std::stoll(num_str, nullptr, 16);
        return Token(TokenType::NUMBER, std::string(m_source.substr(start, m_current - start)),
                    value, loc);
    }

    // Check for 0b prefix (binary)
    // Only treat as binary if there are actual binary digits after the prefix
    if (peek() == '0' && (peekNext() == 'b' || peekNext() == 'B')) {
        // Look ahead to see if there are binary digits
        size_t check_pos = m_current + 2;
        if (check_pos < m_source.size() && (m_source[check_pos] == '0' || m_source[check_pos] == '1')) {
            advance(); // '0'
            advance(); // 'b'
            while (peek() == '0' || peek() == '1') {
                advance();
            }
            std::string num_str(m_source.substr(start + 2, m_current - start - 2));
            int64_t value = std::stoll(num_str, nullptr, 2);
            return Token(TokenType::NUMBER, std::string(m_source.substr(start, m_current - start)),
                        value, loc);
        }
        // If no binary digits follow, fall through to hex suffix parsing
    }

    // Check for 0o prefix (octal)
    // Only treat as octal if there are actual octal digits after the prefix
    if (peek() == '0' && (peekNext() == 'o' || peekNext() == 'O')) {
        // Look ahead to see if there are octal digits
        size_t check_pos = m_current + 2;
        if (check_pos < m_source.size() && m_source[check_pos] >= '0' && m_source[check_pos] <= '7') {
            advance(); // '0'
            advance(); // 'o'
            while (peek() >= '0' && peek() <= '7') {
                advance();
            }
            std::string num_str(m_source.substr(start + 2, m_current - start - 2));
            int64_t value = std::stoll(num_str, nullptr, 8);
            return Token(TokenType::NUMBER, std::string(m_source.substr(start, m_current - start)),
                        value, loc);
        }
        // If no octal digits follow, fall through to hex suffix parsing
    }

    // Scan digits (could be hex with suffix)
    while (isHexDigit(peek())) {
        advance();
    }

    char suffix = peek();
    if (suffix == 'h' || suffix == 'H') {
        // Hex suffix
        advance();
        std::string num_str(m_source.substr(start, m_current - start - 1));
        int64_t value = std::stoll(num_str, nullptr, 16);
        return Token(TokenType::NUMBER, std::string(m_source.substr(start, m_current - start)),
                    value, loc);
    } else if (suffix == 'b' || suffix == 'B') {
        // Binary suffix
        advance();
        std::string num_str(m_source.substr(start, m_current - start - 1));
        int64_t value = std::stoll(num_str, nullptr, 2);
        return Token(TokenType::NUMBER, std::string(m_source.substr(start, m_current - start)),
                    value, loc);
    } else if (suffix == 'o' || suffix == 'O' || suffix == 'q' || suffix == 'Q') {
        // Octal suffix
        advance();
        std::string num_str(m_source.substr(start, m_current - start - 1));
        int64_t value = std::stoll(num_str, nullptr, 8);
        return Token(TokenType::NUMBER, std::string(m_source.substr(start, m_current - start)),
                    value, loc);
    }

    // Default is decimal
    std::string num_str(m_source.substr(start, m_current - start));
    int64_t value = std::stoll(num_str, nullptr, 10);
    return Token(TokenType::NUMBER, num_str, value, loc);
}

Token Lexer::scanIdentifier() {
    SourceLocation loc = currentLocation();
    size_t start = m_current;

    // Handle preprocessor directives starting with %
    if (peek() == '%') {
        advance();
        while (isAlpha(peek()) || peek() == '_') {
            advance();
        }
        std::string text(m_source.substr(start, m_current - start));
        std::string upper = toUpper(text);

        // TODO: Create a static LUT for the macros
        if (upper == "%DEFINE") return Token(TokenType::PREP_DEFINE, text, loc);
        if (upper == "%MACRO") return Token(TokenType::PREP_MACRO, text, loc);
        if (upper == "%ENDMACRO") return Token(TokenType::PREP_ENDMACRO, text, loc);
        if (upper == "%IF") return Token(TokenType::PREP_IF, text, loc);
        if (upper == "%ELIF") return Token(TokenType::PREP_ELIF, text, loc);
        if (upper == "%ELSE") return Token(TokenType::PREP_ELSE, text, loc);
        if (upper == "%ENDIF") return Token(TokenType::PREP_ENDIF, text, loc);
        if (upper == "%IFDEF") return Token(TokenType::PREP_IFDEF, text, loc);
        if (upper == "%IFNDEF") return Token(TokenType::PREP_IFNDEF, text, loc);
        if (upper == "%INCLUDE") return Token(TokenType::PREP_INCLUDE, text, loc);

        return Token(TokenType::IDENTIFIER, text, loc);
    }

    // Regular identifier
    while (isAlphaNumeric(peek()) || peek() == '_' || peek() == '.') {
        advance();
    }

    std::string text(m_source.substr(start, m_current - start));
    std::string upper = toUpper(text);

    // Check if it's a register
    auto reg_it = s_registers.find(upper);
    if (reg_it != s_registers.end()) {
        return Token(reg_it->second, text, loc);
    }

    // Check if it's a keyword
    auto kw_it = s_keywords.find(upper);
    if (kw_it != s_keywords.end()) {
        return Token(kw_it->second, text, loc);
    }

    // Check if it's an instruction
    // BUT: If followed by a colon, treat as label (identifier) instead
    if (s_instructions.count(upper) > 0) {
        // Look ahead to see if there's a colon (label definition)
        if (peek() != ':') {
            return Token(TokenType::INSTRUCTION, text, loc);
        }
        // If followed by colon, fall through to identifier (label)
    }

    // Default is identifier (label or symbol)
    return Token(TokenType::IDENTIFIER, text, loc);
}

Token Lexer::scanString() {
    SourceLocation loc = currentLocation();
    advance(); // The opening "

    std::string value;
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (isAtEnd()) break;

            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                case 'x': {
                    // Hex escape \xHH
                    if (isHexDigit(peek()) && isHexDigit(peekNext())) {
                        char hex[3] = {advance(), advance(), '\0'};
                        value += static_cast<char>(std::stoi(hex, nullptr, 16));
                    }
                    break;
                }
                default: value += escaped; break;
            }
        } else {
            value += advance();
        }
    }

    if (!isAtEnd()) {
        advance(); // The closing "
    }

    return Token(TokenType::STRING, value, value, loc);
}

Token Lexer::scanCharacter() {
    SourceLocation loc = currentLocation();
    advance(); // The opening '

    // NASM strings style support (using single and double quotes)
    std::string value;
    while (!isAtEnd() && peek() != '\'') {
        if (peek() == '\\') {
            advance();
            if (isAtEnd()) break;

            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                case 'x': {
                    // Hex escape \xHH
                    if (isHexDigit(peek()) && isHexDigit(peekNext())) {
                        char hex[3] = {advance(), advance(), '\0'};
                        value += static_cast<char>(std::stoi(hex, nullptr, 16));
                    }
                    break;
                }
                default: value += escaped; break;
            }
        } else {
            value += advance();
        }
    }

    if (!isAtEnd()) {
        advance(); // The closing '
    }

    // If it's a single character, return as NUMBER token (for compatibility)
    if (value.length() == 1) {
        int64_t num_value = static_cast<unsigned char>(value[0]);
        return Token(TokenType::NUMBER, "'" + value + "'", num_value, loc);
    }

    // Multi-character string
    return Token(TokenType::STRING, value, value, loc);
}

bool Lexer::isAtEnd() const {
    return m_current >= m_source.size();
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return m_source[m_current];
}

char Lexer::peekNext() const {
    if (m_current + 1 >= m_source.size()) return '\0';
    return m_source[m_current + 1];
}

char Lexer::advance() {
    if (isAtEnd()) return '\0';
    char c = m_source[m_current++];
    advanceLocation(c);
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (peek() != expected) return false;
    advance();
    return true;
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else {
            break;
        }
    }
}

void Lexer::skipLineComment() {
    // Skip until newline
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

SourceLocation Lexer::currentLocation() const {
    return SourceLocation(m_filename, m_line, m_column);
}

void Lexer::advanceLocation(char c) {
    if (c == '\n') {
        m_line++;
        m_column = 1;
    } else {
        m_column++;
    }
}

bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::isHexDigit(char c) const {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

bool Lexer::isAlpha(char c) const {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

bool Lexer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

} // namespace e2asm
