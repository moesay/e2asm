#include <gtest/gtest.h>
#include "e2asm/lexer/lexer.h"
#include "e2asm/parser/parser.h"

using namespace e2asm;

class ParserTest : public ::testing::Test {
protected:
    std::unique_ptr<Program> parse(const std::string& source) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        return parser.parse();
    }

    bool parseSucceeds(const std::string& source) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto program = parser.parse();
        return program != nullptr && !parser.hasErrors();
    }
};

TEST_F(ParserTest, EmptyProgram) {
    auto program = parse("");
    ASSERT_NE(program, nullptr);
    EXPECT_TRUE(program->statements.empty());
}

TEST_F(ParserTest, SingleNOP) {
    auto program = parse("NOP");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 1);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "NOP");
    EXPECT_TRUE(instr->operands.empty());
}

TEST_F(ParserTest, SimpleLabel) {
    auto program = parse("start:");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 1);

    auto* label = dynamic_cast<Label*>(program->statements[0].get());
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->name, "start");
}

TEST_F(ParserTest, LocalLabel) {
    auto program = parse(".loop:");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 1);

    auto* label = dynamic_cast<Label*>(program->statements[0].get());
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->name, ".loop");
}

TEST_F(ParserTest, LabelWithInstruction) {
    auto program = parse("start: NOP");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 2);

    auto* label = dynamic_cast<Label*>(program->statements[0].get());
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->name, "start");

    auto* instr = dynamic_cast<Instruction*>(program->statements[1].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "NOP");
}

TEST_F(ParserTest, MovRegReg) {
    auto program = parse("MOV AX, BX");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 1);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "MOV");
    ASSERT_EQ(instr->operands.size(), 2);

    auto* dest = dynamic_cast<RegisterOperand*>(instr->operands[0].get());
    ASSERT_NE(dest, nullptr);
    EXPECT_EQ(dest->name, "AX");
    EXPECT_EQ(dest->size, 16);

    auto* src = dynamic_cast<RegisterOperand*>(instr->operands[1].get());
    ASSERT_NE(src, nullptr);
    EXPECT_EQ(src->name, "BX");
    EXPECT_EQ(src->size, 16);
}

TEST_F(ParserTest, MovReg8Reg8) {
    auto program = parse("MOV AL, BL");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 1);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    ASSERT_EQ(instr->operands.size(), 2);

    auto* dest = dynamic_cast<RegisterOperand*>(instr->operands[0].get());
    ASSERT_NE(dest, nullptr);
    EXPECT_EQ(dest->size, 8);
}

TEST_F(ParserTest, MovRegImm) {
    auto program = parse("MOV AX, 42");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 1);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    ASSERT_EQ(instr->operands.size(), 2);

    auto* imm = dynamic_cast<ImmediateOperand*>(instr->operands[1].get());
    ASSERT_NE(imm, nullptr);
    EXPECT_EQ(imm->value, 42);
}

TEST_F(ParserTest, MovRegHexImm) {
    auto program = parse("MOV AX, 0x1234");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);

    auto* imm = dynamic_cast<ImmediateOperand*>(instr->operands[1].get());
    ASSERT_NE(imm, nullptr);
    EXPECT_EQ(imm->value, 0x1234);
}

TEST_F(ParserTest, MovRegMemDirect) {
    auto program = parse("MOV AX, [1234h]");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    ASSERT_EQ(instr->operands.size(), 2);

    auto* mem = dynamic_cast<MemoryOperand*>(instr->operands[1].get());
    ASSERT_NE(mem, nullptr);
}

TEST_F(ParserTest, MovRegMemRegister) {
    auto program = parse("MOV AX, [BX]");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    ASSERT_EQ(instr->operands.size(), 2);

    auto* mem = dynamic_cast<MemoryOperand*>(instr->operands[1].get());
    ASSERT_NE(mem, nullptr);
}

TEST_F(ParserTest, MovRegMemComplex) {
    auto program = parse("MOV AX, [BX+SI+10]");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);

    auto* mem = dynamic_cast<MemoryOperand*>(instr->operands[1].get());
    ASSERT_NE(mem, nullptr);
}

TEST_F(ParserTest, MemoryWithSizeHint) {
    auto program = parse("MOV BYTE [BX], 0");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);

    auto* mem = dynamic_cast<MemoryOperand*>(instr->operands[0].get());
    ASSERT_NE(mem, nullptr);
    EXPECT_EQ(mem->size_hint, 8);
}

TEST_F(ParserTest, MemoryWithWordSizeHint) {
    auto program = parse("MOV WORD [BX], 0");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);

    auto* mem = dynamic_cast<MemoryOperand*>(instr->operands[0].get());
    ASSERT_NE(mem, nullptr);
    EXPECT_EQ(mem->size_hint, 16);
}

TEST_F(ParserTest, MemoryWithSegmentOverride) {
    auto program = parse("MOV AX, [ES:BX]");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);

    auto* mem = dynamic_cast<MemoryOperand*>(instr->operands[1].get());
    ASSERT_NE(mem, nullptr);
    ASSERT_TRUE(mem->segment_override.has_value());
    EXPECT_EQ(mem->segment_override.value(), "ES");
}

TEST_F(ParserTest, JmpLabel) {
    auto program = parse("JMP start");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "JMP");
    ASSERT_EQ(instr->operands.size(), 1);

    auto* label_ref = dynamic_cast<LabelRef*>(instr->operands[0].get());
    ASSERT_NE(label_ref, nullptr);
    EXPECT_EQ(label_ref->label, "start");
}

TEST_F(ParserTest, JmpShortLabel) {
    auto program = parse("JMP SHORT .loop");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);

    auto* label_ref = dynamic_cast<LabelRef*>(instr->operands[0].get());
    ASSERT_NE(label_ref, nullptr);
    EXPECT_EQ(label_ref->jump_type, LabelRef::JumpType::SHORT);
}

TEST_F(ParserTest, ConditionalJump) {
    auto program = parse("JE done");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "JE");
}

TEST_F(ParserTest, CallInstruction) {
    auto program = parse("CALL my_function");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "CALL");
}

TEST_F(ParserTest, PushReg) {
    auto program = parse("PUSH AX");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "PUSH");
    ASSERT_EQ(instr->operands.size(), 1);
}

TEST_F(ParserTest, PopReg) {
    auto program = parse("POP BX");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "POP");
}

TEST_F(ParserTest, DBDirective) {
    auto program = parse("DB 0x55");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 1);

    auto* data = dynamic_cast<DataDirective*>(program->statements[0].get());
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->size, DataDirective::Size::BYTE);
    ASSERT_EQ(data->values.size(), 1);
    EXPECT_EQ(data->values[0].number_value, 0x55);
}

TEST_F(ParserTest, DWDirective) {
    auto program = parse("DW 0x1234");
    ASSERT_NE(program, nullptr);

    auto* data = dynamic_cast<DataDirective*>(program->statements[0].get());
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->size, DataDirective::Size::WORD);
}

TEST_F(ParserTest, DBMultipleValues) {
    auto program = parse("DB 1, 2, 3, 4");
    ASSERT_NE(program, nullptr);

    auto* data = dynamic_cast<DataDirective*>(program->statements[0].get());
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->values.size(), 4);
}

TEST_F(ParserTest, DBString) {
    auto program = parse("DB \"Hello\"");
    ASSERT_NE(program, nullptr);

    auto* data = dynamic_cast<DataDirective*>(program->statements[0].get());
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->values.size(), 1);
    EXPECT_EQ(data->values[0].type, DataValue::Type::STRING);
    EXPECT_EQ(data->values[0].string_value, "Hello");
}

TEST_F(ParserTest, DBStringWithNull) {
    auto program = parse("DB \"Hello\", 0");
    ASSERT_NE(program, nullptr);

    auto* data = dynamic_cast<DataDirective*>(program->statements[0].get());
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->values.size(), 2);
}

TEST_F(ParserTest, LabeledData) {
    auto program = parse("msg: DB \"Hello\", 0");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 2);

    auto* label = dynamic_cast<Label*>(program->statements[0].get());
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->name, "msg");

    auto* data = dynamic_cast<DataDirective*>(program->statements[1].get());
    ASSERT_NE(data, nullptr);
}

TEST_F(ParserTest, EQUDirective) {
    auto program = parse("SCREEN_WIDTH EQU 80");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 1);

    auto* equ = dynamic_cast<EQUDirective*>(program->statements[0].get());
    ASSERT_NE(equ, nullptr);
    EXPECT_EQ(equ->name, "SCREEN_WIDTH");
    EXPECT_EQ(equ->value, 80);
}

TEST_F(ParserTest, ORGDirective) {
    auto program = parse("ORG 0x7C00");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 1);

    auto* org = dynamic_cast<ORGDirective*>(program->statements[0].get());
    ASSERT_NE(org, nullptr);
    EXPECT_EQ(org->address, 0x7C00);
}

TEST_F(ParserTest, SEGMENTDirective) {
    auto program = parse("SEGMENT .text");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 1);

    auto* seg = dynamic_cast<SEGMENTDirective*>(program->statements[0].get());
    ASSERT_NE(seg, nullptr);
    EXPECT_EQ(seg->name, ".text");
}

TEST_F(ParserTest, RESBDirective) {
    auto program = parse("RESB 512");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 1);

    auto* res = dynamic_cast<RESDirective*>(program->statements[0].get());
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->size, RESDirective::Size::BYTE);
    EXPECT_EQ(res->count, 512);
}

TEST_F(ParserTest, RESWDirective) {
    auto program = parse("RESW 100");
    ASSERT_NE(program, nullptr);

    auto* res = dynamic_cast<RESDirective*>(program->statements[0].get());
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->size, RESDirective::Size::WORD);
    EXPECT_EQ(res->count, 100);
}

TEST_F(ParserTest, TIMESDirective) {
    auto program = parse("TIMES 10 DB 0");
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->statements.size(), 1);

    auto* times = dynamic_cast<TIMESDirective*>(program->statements[0].get());
    ASSERT_NE(times, nullptr);
    EXPECT_EQ(times->count, 10);

    auto* repeated = dynamic_cast<DataDirective*>(times->repeated_node.get());
    ASSERT_NE(repeated, nullptr);
}

TEST_F(ParserTest, TIMESWithNOP) {
    auto program = parse("TIMES 5 NOP");
    ASSERT_NE(program, nullptr);

    auto* times = dynamic_cast<TIMESDirective*>(program->statements[0].get());
    ASSERT_NE(times, nullptr);
    EXPECT_EQ(times->count, 5);

    auto* repeated = dynamic_cast<Instruction*>(times->repeated_node.get());
    ASSERT_NE(repeated, nullptr);
    EXPECT_EQ(repeated->mnemonic, "NOP");
}

TEST_F(ParserTest, AddRegReg) {
    auto program = parse("ADD AX, BX");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "ADD");
    EXPECT_EQ(instr->operands.size(), 2);
}

TEST_F(ParserTest, SubRegImm) {
    auto program = parse("SUB AX, 10");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "SUB");
}

TEST_F(ParserTest, AndRegReg) {
    auto program = parse("AND AX, BX");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "AND");
}

TEST_F(ParserTest, OrRegImm) {
    auto program = parse("OR AL, 0x0F");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "OR");
}

TEST_F(ParserTest, ShlReg) {
    auto program = parse("SHL AX, 1");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "SHL");
}

TEST_F(ParserTest, ShrRegCL) {
    auto program = parse("SHR BX, CL");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "SHR");
}

TEST_F(ParserTest, StringInstructions) {
    EXPECT_TRUE(parseSucceeds("MOVSB"));
    EXPECT_TRUE(parseSucceeds("MOVSW"));
    EXPECT_TRUE(parseSucceeds("STOSB"));
    EXPECT_TRUE(parseSucceeds("LODSW"));
    EXPECT_TRUE(parseSucceeds("REP MOVSB"));
}

TEST_F(ParserTest, IntInstruction) {
    auto program = parse("INT 0x21");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "INT");
    ASSERT_EQ(instr->operands.size(), 1);

    auto* imm = dynamic_cast<ImmediateOperand*>(instr->operands[0].get());
    ASSERT_NE(imm, nullptr);
    EXPECT_EQ(imm->value, 0x21);
}

TEST_F(ParserTest, LeaInstruction) {
    auto program = parse("LEA BX, [SI+10]");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "LEA");
    EXPECT_EQ(instr->operands.size(), 2);
}

TEST_F(ParserTest, CompleteProgram) {
    std::string source = R"(
        ORG 0x7C00

        start:
            MOV AX, 0
            MOV DS, AX
            MOV ES, AX

        .loop:
            MOV AH, 0x0E
            MOV AL, 'A'
            INT 0x10
            JMP .loop

        TIMES 510-($-$$) DB 0
        DW 0xAA55
    )";

    auto program = parse(source);
    ASSERT_NE(program, nullptr);
    EXPECT_GT(program->statements.size(), 5);
}

TEST_F(ParserTest, InvalidInstruction) {
    Lexer lexer("INVALID_INSTR AX, BX");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parse();
}

TEST_F(ParserTest, MissingOperand) {
    Lexer lexer("MOV AX,");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parse();
}

TEST_F(ParserTest, InInstruction) {
    auto program = parse("IN AL, DX");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "IN");
}

TEST_F(ParserTest, OutInstruction) {
    auto program = parse("OUT DX, AL");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "OUT");
}

TEST_F(ParserTest, IncReg) {
    auto program = parse("INC AX");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "INC");
    EXPECT_EQ(instr->operands.size(), 1);
}

TEST_F(ParserTest, DecMem) {
    auto program = parse("DEC WORD [BX]");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "DEC");
}

TEST_F(ParserTest, NegReg) {
    auto program = parse("NEG AX");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "NEG");
}

TEST_F(ParserTest, NotReg) {
    auto program = parse("NOT BX");
    ASSERT_NE(program, nullptr);

    auto* instr = dynamic_cast<Instruction*>(program->statements[0].get());
    ASSERT_NE(instr, nullptr);
    EXPECT_EQ(instr->mnemonic, "NOT");
}
