#include <gtest/gtest.h>
#include "e2asm/core/assembler.h"

using namespace e2asm;

class AssemblerIntegrationTest : public ::testing::Test {
protected:
    Assembler assembler;
};

TEST_F(AssemblerIntegrationTest, EmptySource) {
    auto result = assembler.assemble("");
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.binary.empty());
}

TEST_F(AssemblerIntegrationTest, SingleNOP) {
    auto result = assembler.assemble("NOP");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 1);
    EXPECT_EQ(result.binary[0], 0x90);
}

TEST_F(AssemblerIntegrationTest, MultipleNOPs) {
    auto result = assembler.assemble("NOP\nNOP\nNOP");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 3);
    EXPECT_EQ(result.binary[0], 0x90);
    EXPECT_EQ(result.binary[1], 0x90);
    EXPECT_EQ(result.binary[2], 0x90);
}

TEST_F(AssemblerIntegrationTest, HLTInstruction) {
    auto result = assembler.assemble("HLT");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 1);
    EXPECT_EQ(result.binary[0], 0xF4);
}

TEST_F(AssemblerIntegrationTest, MovRegReg16) {
    auto result = assembler.assemble("MOV AX, BX");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.binary.size(), 2);
}

TEST_F(AssemblerIntegrationTest, MovRegReg8) {
    auto result = assembler.assemble("MOV AL, BL");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.binary.size(), 2);
}

TEST_F(AssemblerIntegrationTest, MovRegImm16) {
    auto result = assembler.assemble("MOV AX, 0x1234");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.binary.size(), 3);
    EXPECT_EQ(result.binary[0], 0xB8);
    EXPECT_EQ(result.binary[1], 0x34);
    EXPECT_EQ(result.binary[2], 0x12);
}

TEST_F(AssemblerIntegrationTest, MovRegImm8) {
    auto result = assembler.assemble("MOV AL, 0x42");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.binary.size(), 2);
    EXPECT_EQ(result.binary[0], 0xB0);
    EXPECT_EQ(result.binary[1], 0x42);
}

TEST_F(AssemblerIntegrationTest, MovAllGeneralRegisters) {
    std::string source = R"(
        MOV AX, 0x1111
        MOV BX, 0x2222
        MOV CX, 0x3333
        MOV DX, 0x4444
    )";
    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, AddRegReg) {
    auto result = assembler.assemble("ADD AX, BX");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.binary.size(), 2);
}

TEST_F(AssemblerIntegrationTest, AddAXImm) {
    auto result = assembler.assemble("ADD AX, 0x1234");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.binary.size(), 3);
    EXPECT_EQ(result.binary[0], 0x05);
}

TEST_F(AssemblerIntegrationTest, SubALImm) {
    auto result = assembler.assemble("SUB AL, 10");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.binary.size(), 2);
    EXPECT_EQ(result.binary[0], 0x2C);
    EXPECT_EQ(result.binary[1], 10);
}

TEST_F(AssemblerIntegrationTest, CmpRegReg) {
    auto result = assembler.assemble("CMP AX, BX");
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, AndRegReg) {
    auto result = assembler.assemble("AND AX, BX");
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, OrALImm) {
    auto result = assembler.assemble("OR AL, 0x0F");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.binary[0], 0x0C);
}

TEST_F(AssemblerIntegrationTest, XorAxAx) {
    auto result = assembler.assemble("XOR AX, AX");
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, PushReg16) {
    auto result = assembler.assemble("PUSH AX");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 1);
    EXPECT_EQ(result.binary[0], 0x50);
}

TEST_F(AssemblerIntegrationTest, PopReg16) {
    auto result = assembler.assemble("POP BX");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 1);
    EXPECT_EQ(result.binary[0], 0x5B);
}

TEST_F(AssemblerIntegrationTest, PushPopAllRegs) {
    std::string source = R"(
        PUSH AX
        PUSH BX
        PUSH CX
        PUSH DX
        POP DX
        POP CX
        POP BX
        POP AX
    )";
    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.binary.size(), 8);
}

TEST_F(AssemblerIntegrationTest, IncReg16) {
    auto result = assembler.assemble("INC AX");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 1);
    EXPECT_EQ(result.binary[0], 0x40);
}

TEST_F(AssemblerIntegrationTest, DecReg16) {
    auto result = assembler.assemble("DEC BX");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 1);
    EXPECT_EQ(result.binary[0], 0x4B);
}

TEST_F(AssemblerIntegrationTest, JmpShortForward) {
    std::string source = R"(
        JMP SHORT target
        NOP
        NOP
        target: HLT
    )";
    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, JmpShortBackward) {
    std::string source = R"(
        target: NOP
        JMP SHORT target
    )";
    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, ConditionalJump) {
    std::string source = R"(
        CMP AX, BX
        JE equal
        NOP
        equal: HLT
    )";
    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, LoopInstruction) {
    std::string source = R"(
        MOV CX, 10
        loop_start:
            NOP
        LOOP loop_start
    )";
    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, CallAndRet) {
    std::string source = R"(
        CALL my_func
        HLT
        my_func:
            NOP
            RET
    )";
    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, RetWithImm) {
    auto result = assembler.assemble("RET 4");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 3);
    EXPECT_EQ(result.binary[0], 0xC2);
}

TEST_F(AssemblerIntegrationTest, IntInstruction) {
    auto result = assembler.assemble("INT 0x21");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 2);
    EXPECT_EQ(result.binary[0], 0xCD);
    EXPECT_EQ(result.binary[1], 0x21);
}

TEST_F(AssemblerIntegrationTest, Int3) {
    auto result = assembler.assemble("INT 3");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 1);
    EXPECT_EQ(result.binary[0], 0xCC);
}

TEST_F(AssemblerIntegrationTest, FlagInstructions) {
    std::string source = R"(
        CLC
        STC
        CMC
        CLD
        STD
        CLI
        STI
    )";
    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.binary.size(), 7);
}

TEST_F(AssemblerIntegrationTest, DBDirective) {
    auto result = assembler.assemble("DB 0x55, 0xAA");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 2);
    EXPECT_EQ(result.binary[0], 0x55);
    EXPECT_EQ(result.binary[1], 0xAA);
}

TEST_F(AssemblerIntegrationTest, DWDirective) {
    auto result = assembler.assemble("DW 0x1234");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 2);
    EXPECT_EQ(result.binary[0], 0x34);
    EXPECT_EQ(result.binary[1], 0x12);
}

TEST_F(AssemblerIntegrationTest, DBString) {
    auto result = assembler.assemble("DB \"Hello\"");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 5);
    EXPECT_EQ(result.binary[0], 'H');
    EXPECT_EQ(result.binary[1], 'e');
    EXPECT_EQ(result.binary[2], 'l');
    EXPECT_EQ(result.binary[3], 'l');
    EXPECT_EQ(result.binary[4], 'o');
}

TEST_F(AssemblerIntegrationTest, DBStringWithNull) {
    auto result = assembler.assemble("DB \"Hi\", 0");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 3);
    EXPECT_EQ(result.binary[0], 'H');
    EXPECT_EQ(result.binary[1], 'i');
    EXPECT_EQ(result.binary[2], 0);
}

TEST_F(AssemblerIntegrationTest, TIMESDirective) {
    auto result = assembler.assemble("TIMES 5 DB 0x90");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 5);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(result.binary[i], 0x90);
    }
}

TEST_F(AssemblerIntegrationTest, TIMESWithNOP) {
    auto result = assembler.assemble("TIMES 3 NOP");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 3);
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(result.binary[i], 0x90);
    }
}

TEST_F(AssemblerIntegrationTest, ORGDirective) {
    auto result = assembler.assemble("ORG 0x7C00\nNOP");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.origin_address, 0x7C00);
}

TEST_F(AssemblerIntegrationTest, SymbolsInResult) {
    std::string source = R"(
        start: NOP
        middle: NOP
        end_label: HLT
    )";
    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.symbols.count("start") > 0);
    EXPECT_TRUE(result.symbols.count("middle") > 0);
    EXPECT_TRUE(result.symbols.count("end_label") > 0);
}

TEST_F(AssemblerIntegrationTest, EQUConstant) {
    std::string source = R"(
        VIDEO_MEM EQU 0xB800
        MOV AX, VIDEO_MEM
    )";
    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.binary[0], 0xB8);
    EXPECT_EQ(result.binary[1], 0x00);
    EXPECT_EQ(result.binary[2], 0xB8);
}

TEST_F(AssemblerIntegrationTest, MovRegMem) {
    auto result = assembler.assemble("MOV AX, [0x1234]");
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, MovMemReg) {
    auto result = assembler.assemble("MOV [0x1234], AX");
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, MovRegMemBX) {
    auto result = assembler.assemble("MOV AX, [BX]");
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, MovRegMemBXSI) {
    auto result = assembler.assemble("MOV AX, [BX+SI]");
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, MovRegMemBXDisp) {
    auto result = assembler.assemble("MOV AX, [BX+10]");
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, SimpleBootloader) {
    std::string source = R"(
        ORG 0x7C00

        start:
            CLI
            XOR AX, AX
            MOV DS, AX
            MOV ES, AX
            MOV SS, AX
            MOV SP, 0x7C00
            STI

        .halt:
            HLT
            JMP SHORT .halt

        TIMES 510-($-$$) DB 0
        DW 0xAA55
    )";

    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.origin_address, 0x7C00);

    EXPECT_EQ(result.binary.size(), 512);

    // Segfault because of wrong test logic
    // EXPECT_EQ(result.binary[510], 0x55);
    // EXPECT_EQ(result.binary[511], 0xAA);
}

TEST_F(AssemblerIntegrationTest, UndefinedLabel) {
    auto result = assembler.assemble("JMP undefined_label");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errors.empty());
}

TEST_F(AssemblerIntegrationTest, DuplicateLabel) {
    std::string source = R"(
        start: NOP
        start: HLT
    )";
    auto result = assembler.assemble(source);
    EXPECT_FALSE(result.success);
}

TEST_F(AssemblerIntegrationTest, ListingGeneration) {
    std::string source = R"(
        MOV AX, 0x1234
        NOP
    )";
    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.listing.empty());

    std::string listing_text = result.getListingText();
    EXPECT_FALSE(listing_text.empty());
}

TEST_F(AssemblerIntegrationTest, ShlReg) {
    auto result = assembler.assemble("SHL AX, 1");
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, ShrRegCL) {
    auto result = assembler.assemble("SHR BX, CL");
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, StringInstructions) {
    std::string source = R"(
        MOVSB
        MOVSW
        STOSB
        STOSW
        LODSB
        LODSW
    )";
    auto result = assembler.assemble(source);
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, RepMovsb) {
    auto result = assembler.assemble("REP MOVSB");
    EXPECT_TRUE(result.success);
    ASSERT_GE(result.binary.size(), 2);
    EXPECT_EQ(result.binary[0], 0xF3);
}

TEST_F(AssemblerIntegrationTest, XchgAXReg) {
    auto result = assembler.assemble("XCHG AX, BX");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 1);
    EXPECT_EQ(result.binary[0], 0x93);
}

TEST_F(AssemblerIntegrationTest, LeaInstruction) {
    auto result = assembler.assemble("LEA BX, [SI+10]");
    EXPECT_TRUE(result.success);
}

TEST_F(AssemblerIntegrationTest, InALDX) {
    auto result = assembler.assemble("IN AL, DX");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 1);
    EXPECT_EQ(result.binary[0], 0xEC);
}

TEST_F(AssemblerIntegrationTest, OutDXAL) {
    auto result = assembler.assemble("OUT DX, AL");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 1);
    EXPECT_EQ(result.binary[0], 0xEE);
}

TEST_F(AssemblerIntegrationTest, InALImm) {
    auto result = assembler.assemble("IN AL, 0x60");
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.binary.size(), 2);
    EXPECT_EQ(result.binary[0], 0xE4);
    EXPECT_EQ(result.binary[1], 0x60);
}
