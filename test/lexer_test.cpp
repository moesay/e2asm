#include <gtest/gtest.h>
#include "e2asm/lexer/lexer.h"

using namespace e2asm;

class LexerTest : public ::testing::Test {
protected:
    std::vector<Token> tokenize(const std::string& source) {
        Lexer lexer(source);
        return lexer.tokenize();
    }
};

TEST_F(LexerTest, EmptyInput) {
    auto tokens = tokenize("");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::END_OF_FILE);
}

TEST_F(LexerTest, WhitespaceOnly) {
    auto tokens = tokenize("   \t  ");
    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::END_OF_FILE);
}

TEST_F(LexerTest, SingleLineComment) {
    auto tokens = tokenize("; this is a comment");
    ASSERT_EQ(tokens.size(), 2);

    EXPECT_EQ(tokens[0].type, TokenType::NEWLINE);
    EXPECT_EQ(tokens[1].type, TokenType::END_OF_FILE);
}

TEST_F(LexerTest, NewlineToken) {
    auto tokens = tokenize("MOV\nADD");
    ASSERT_GE(tokens.size(), 3);
    EXPECT_EQ(tokens[0].type, TokenType::INSTRUCTION);
    EXPECT_EQ(tokens[1].type, TokenType::NEWLINE);
    EXPECT_EQ(tokens[2].type, TokenType::INSTRUCTION);
}

TEST_F(LexerTest, DecimalNumber) {
    auto tokens = tokenize("42");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].getNumber(), 42);
}

TEST_F(LexerTest, HexNumberWithPrefix0x) {
    auto tokens = tokenize("0x2A");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].getNumber(), 0x2A);
}

TEST_F(LexerTest, HexNumberWithSuffix) {
    auto tokens = tokenize("2Ah");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].getNumber(), 0x2A);
}

TEST_F(LexerTest, BinaryNumber) {
    auto tokens = tokenize("0b101010");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].getNumber(), 42);
}

TEST_F(LexerTest, OctalNumber) {
    auto tokens = tokenize("52o");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].getNumber(), 42);
}

TEST_F(LexerTest, DoubleQuotedString) {
    auto tokens = tokenize("\"hello\"");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::STRING);
    EXPECT_EQ(tokens[0].getString(), "hello");
}

TEST_F(LexerTest, SingleQuotedString) {
    auto tokens = tokenize("'world'");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::STRING);
}

TEST_F(LexerTest, CharacterLiteral) {
    auto tokens = tokenize("'A'");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[0].getNumber(), 65);
}

TEST_F(LexerTest, Register8Bit) {
    auto tokens = tokenize("AL");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::REG8_AL);
    EXPECT_TRUE(tokens[0].isReg8());
}

TEST_F(LexerTest, Register16Bit) {
    auto tokens = tokenize("AX");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::REG16_AX);
    EXPECT_TRUE(tokens[0].isReg16());
}

TEST_F(LexerTest, AllGeneralPurposeRegisters) {
    std::vector<std::pair<std::string, TokenType>> regs = {
        {"AL", TokenType::REG8_AL}, {"CL", TokenType::REG8_CL},
        {"DL", TokenType::REG8_DL}, {"BL", TokenType::REG8_BL},
        {"AH", TokenType::REG8_AH}, {"CH", TokenType::REG8_CH},
        {"DH", TokenType::REG8_DH}, {"BH", TokenType::REG8_BH},
        {"AX", TokenType::REG16_AX}, {"CX", TokenType::REG16_CX},
        {"DX", TokenType::REG16_DX}, {"BX", TokenType::REG16_BX},
        {"SP", TokenType::REG16_SP}, {"BP", TokenType::REG16_BP},
        {"SI", TokenType::REG16_SI}, {"DI", TokenType::REG16_DI}
    };

    for (const auto& [name, expected_type] : regs) {
        auto tokens = tokenize(name);
        ASSERT_GE(tokens.size(), 1) << "Failed for register: " << name;
        EXPECT_EQ(tokens[0].type, expected_type) << "Failed for register: " << name;
    }
}

TEST_F(LexerTest, SegmentRegisters) {
    std::vector<std::pair<std::string, TokenType>> regs = {
        {"ES", TokenType::SEGREG_ES}, {"CS", TokenType::SEGREG_CS},
        {"SS", TokenType::SEGREG_SS}, {"DS", TokenType::SEGREG_DS}
    };

    for (const auto& [name, expected_type] : regs) {
        auto tokens = tokenize(name);
        ASSERT_GE(tokens.size(), 1) << "Failed for register: " << name;
        EXPECT_EQ(tokens[0].type, expected_type) << "Failed for register: " << name;
        EXPECT_TRUE(tokens[0].isSegReg()) << "Failed for register: " << name;
    }
}

TEST_F(LexerTest, CaseInsensitiveRegisters) {
    auto tokens_upper = tokenize("AX");
    auto tokens_lower = tokenize("ax");
    auto tokens_mixed = tokenize("Ax");

    EXPECT_EQ(tokens_upper[0].type, TokenType::REG16_AX);
    EXPECT_EQ(tokens_lower[0].type, TokenType::REG16_AX);
    EXPECT_EQ(tokens_mixed[0].type, TokenType::REG16_AX);
}

TEST_F(LexerTest, BasicInstructions) {
    std::vector<std::string> instructions = {
        "MOV", "ADD", "SUB", "MUL", "DIV", "JMP", "CALL", "RET",
        "PUSH", "POP", "AND", "OR", "XOR", "NOT", "NOP", "HLT"
    };

    for (const auto& instr : instructions) {
        auto tokens = tokenize(instr);
        ASSERT_GE(tokens.size(), 1) << "Failed for instruction: " << instr;
        EXPECT_EQ(tokens[0].type, TokenType::INSTRUCTION)
            << "Failed for instruction: " << instr;
        EXPECT_EQ(tokens[0].lexeme, instr);
    }
}

TEST_F(LexerTest, ConditionalJumps) {
    std::vector<std::string> jumps = {
        "JE", "JNE", "JZ", "JNZ", "JL", "JLE", "JG", "JGE",
        "JA", "JAE", "JB", "JBE", "JC", "JNC", "JO", "JNO"
    };

    for (const auto& jmp : jumps) {
        auto tokens = tokenize(jmp);
        ASSERT_GE(tokens.size(), 1) << "Failed for jump: " << jmp;
        EXPECT_EQ(tokens[0].type, TokenType::INSTRUCTION)
            << "Failed for jump: " << jmp;
    }
}

TEST_F(LexerTest, CaseInsensitiveInstructions) {
    auto tokens_upper = tokenize("MOV");
    auto tokens_lower = tokenize("mov");
    auto tokens_mixed = tokenize("Mov");

    EXPECT_EQ(tokens_upper[0].type, TokenType::INSTRUCTION);
    EXPECT_EQ(tokens_lower[0].type, TokenType::INSTRUCTION);
    EXPECT_EQ(tokens_mixed[0].type, TokenType::INSTRUCTION);
}

TEST_F(LexerTest, DataDirectives) {
    std::vector<std::pair<std::string, TokenType>> directives = {
        {"DB", TokenType::DIR_DB}, {"DW", TokenType::DIR_DW},
        {"DD", TokenType::DIR_DD}, {"DQ", TokenType::DIR_DQ},
        {"DT", TokenType::DIR_DT}
    };

    for (const auto& [name, expected_type] : directives) {
        auto tokens = tokenize(name);
        ASSERT_GE(tokens.size(), 1) << "Failed for directive: " << name;
        EXPECT_EQ(tokens[0].type, expected_type) << "Failed for directive: " << name;
    }
}

TEST_F(LexerTest, ReserveDirectives) {
    std::vector<std::pair<std::string, TokenType>> directives = {
        {"RESB", TokenType::DIR_RESB}, {"RESW", TokenType::DIR_RESW},
        {"RESD", TokenType::DIR_RESD}, {"RESQ", TokenType::DIR_RESQ},
        {"REST", TokenType::DIR_REST}
    };

    for (const auto& [name, expected_type] : directives) {
        auto tokens = tokenize(name);
        ASSERT_GE(tokens.size(), 1) << "Failed for directive: " << name;
        EXPECT_EQ(tokens[0].type, expected_type) << "Failed for directive: " << name;
    }
}

TEST_F(LexerTest, OtherDirectives) {
    auto equ_tokens = tokenize("EQU");
    EXPECT_EQ(equ_tokens[0].type, TokenType::DIR_EQU);

    auto org_tokens = tokenize("ORG");
    EXPECT_EQ(org_tokens[0].type, TokenType::DIR_ORG);

    auto segment_tokens = tokenize("SEGMENT");
    EXPECT_EQ(segment_tokens[0].type, TokenType::DIR_SEGMENT);

    auto times_tokens = tokenize("TIMES");
    EXPECT_EQ(times_tokens[0].type, TokenType::DIR_TIMES);
}

TEST_F(LexerTest, ArithmeticOperators) {
    auto tokens = tokenize("+ - * / %");
    EXPECT_EQ(tokens[0].type, TokenType::PLUS);
    EXPECT_EQ(tokens[1].type, TokenType::MINUS);
    EXPECT_EQ(tokens[2].type, TokenType::STAR);
    EXPECT_EQ(tokens[3].type, TokenType::SLASH);
    EXPECT_EQ(tokens[4].type, TokenType::PERCENT);
}

TEST_F(LexerTest, BitwiseOperators) {
    auto tokens = tokenize("& | ^ ~");
    EXPECT_EQ(tokens[0].type, TokenType::AND_OP);
    EXPECT_EQ(tokens[1].type, TokenType::OR_OP);
    EXPECT_EQ(tokens[2].type, TokenType::XOR_OP);
    EXPECT_EQ(tokens[3].type, TokenType::TILDE);
}

TEST_F(LexerTest, ShiftOperators) {
    auto tokens = tokenize("<< >>");
    EXPECT_EQ(tokens[0].type, TokenType::SHL_OP);
    EXPECT_EQ(tokens[1].type, TokenType::SHR_OP);
}

TEST_F(LexerTest, Punctuation) {
    auto tokens = tokenize(", : [ ] ( )");
    EXPECT_EQ(tokens[0].type, TokenType::COMMA);
    EXPECT_EQ(tokens[1].type, TokenType::COLON);
    EXPECT_EQ(tokens[2].type, TokenType::LBRACKET);
    EXPECT_EQ(tokens[3].type, TokenType::RBRACKET);
    EXPECT_EQ(tokens[4].type, TokenType::LPAREN);
    EXPECT_EQ(tokens[5].type, TokenType::RPAREN);
}

TEST_F(LexerTest, SpecialMarkers) {
    auto dollar = tokenize("$");
    EXPECT_EQ(dollar[0].type, TokenType::DOLLAR);

    auto double_dollar = tokenize("$$");
    EXPECT_EQ(double_dollar[0].type, TokenType::DOUBLE_DOLLAR);
}

TEST_F(LexerTest, SizeSpecifiers) {
    auto byte_ptr = tokenize("BYTE");
    EXPECT_EQ(byte_ptr[0].type, TokenType::BYTE_PTR);

    auto word_ptr = tokenize("WORD");
    EXPECT_EQ(word_ptr[0].type, TokenType::WORD_PTR);

    auto dword_ptr = tokenize("DWORD");
    EXPECT_EQ(dword_ptr[0].type, TokenType::DWORD_PTR);
}

TEST_F(LexerTest, JumpModifiers) {
    auto short_kw = tokenize("SHORT");
    EXPECT_EQ(short_kw[0].type, TokenType::SHORT_KW);

    auto near_kw = tokenize("NEAR");
    EXPECT_EQ(near_kw[0].type, TokenType::NEAR_KW);

    auto far_kw = tokenize("FAR");
    EXPECT_EQ(far_kw[0].type, TokenType::FAR_KW);
}

TEST_F(LexerTest, SimpleIdentifier) {
    auto tokens = tokenize("my_label");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].lexeme, "my_label");
}

TEST_F(LexerTest, LocalLabel) {
    auto tokens = tokenize(".local_label");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].lexeme, ".local_label");
}

TEST_F(LexerTest, IdentifierWithNumbers) {
    auto tokens = tokenize("label123");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].lexeme, "label123");
}

TEST_F(LexerTest, CompleteInstruction) {
    auto tokens = tokenize("MOV AX, BX");
    EXPECT_EQ(tokens[0].type, TokenType::INSTRUCTION);
    EXPECT_EQ(tokens[0].lexeme, "MOV");
    EXPECT_EQ(tokens[1].type, TokenType::REG16_AX);
    EXPECT_EQ(tokens[2].type, TokenType::COMMA);
    EXPECT_EQ(tokens[3].type, TokenType::REG16_BX);
}

TEST_F(LexerTest, MemoryOperand) {
    auto tokens = tokenize("[BX+SI+10]");
    EXPECT_EQ(tokens[0].type, TokenType::LBRACKET);
    EXPECT_EQ(tokens[1].type, TokenType::REG16_BX);
    EXPECT_EQ(tokens[2].type, TokenType::PLUS);
    EXPECT_EQ(tokens[3].type, TokenType::REG16_SI);
    EXPECT_EQ(tokens[4].type, TokenType::PLUS);
    EXPECT_EQ(tokens[5].type, TokenType::NUMBER);
    EXPECT_EQ(tokens[6].type, TokenType::RBRACKET);
}

TEST_F(LexerTest, LabelDefinition) {
    auto tokens = tokenize("start:");
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[0].lexeme, "start");
    EXPECT_EQ(tokens[1].type, TokenType::COLON);
}

TEST_F(LexerTest, DataDefinition) {
    auto tokens = tokenize("msg DB \"Hello\", 0");
    EXPECT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    EXPECT_EQ(tokens[1].type, TokenType::DIR_DB);
    EXPECT_EQ(tokens[2].type, TokenType::STRING);
    EXPECT_EQ(tokens[3].type, TokenType::COMMA);
    EXPECT_EQ(tokens[4].type, TokenType::NUMBER);
}

TEST_F(LexerTest, MultiLineProgram) {
    std::string source = R"(
        ORG 0x7C00
        start:
            MOV AX, 0
            MOV DS, AX
        .loop:
            JMP .loop
    )";

    auto tokens = tokenize(source);

    EXPECT_GT(tokens.size(), 10);

    EXPECT_EQ(tokens.back().type, TokenType::END_OF_FILE);
}

TEST_F(LexerTest, LocationTracking) {
    auto tokens = tokenize("MOV\nADD");

    EXPECT_EQ(tokens[0].location.line, 1);
    EXPECT_EQ(tokens[2].location.line, 2);
}

TEST_F(LexerTest, PreprocessorDirectives) {
    auto define = tokenize("%define");
    EXPECT_EQ(define[0].type, TokenType::PREP_DEFINE);

    auto include = tokenize("%include");
    EXPECT_EQ(include[0].type, TokenType::PREP_INCLUDE);

    auto ifdef = tokenize("%ifdef");
    EXPECT_EQ(ifdef[0].type, TokenType::PREP_IFDEF);
}
