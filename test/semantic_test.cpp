#include <gtest/gtest.h>
#include "e2asm/lexer/lexer.h"
#include "e2asm/parser/parser.h"
#include "e2asm/semantic/semantic_analyzer.h"
#include "e2asm/semantic/symbol_table.h"

using namespace e2asm;

class SemanticAnalyzerTest : public ::testing::Test {
protected:
    std::unique_ptr<Program> parse(const std::string& source) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        return parser.parse();
    }

    bool analyzeSucceeds(const std::string& source) {
        auto program = parse(source);
        if (!program) return false;

        SemanticAnalyzer analyzer;
        return analyzer.analyze(program.get());
    }

    SemanticAnalyzer analyzeAndGet(const std::string& source) {
        auto program = parse(source);
        SemanticAnalyzer analyzer;
        if (program) {
            analyzer.analyze(program.get());
        }
        return analyzer;
    }
};

TEST_F(SemanticAnalyzerTest, EmptyProgram) {
    EXPECT_TRUE(analyzeSucceeds(""));
}

TEST_F(SemanticAnalyzerTest, SingleInstruction) {
    EXPECT_TRUE(analyzeSucceeds("NOP"));
}

TEST_F(SemanticAnalyzerTest, MultipleInstructions) {
    EXPECT_TRUE(analyzeSucceeds("NOP\nNOP\nHLT"));
}

TEST_F(SemanticAnalyzerTest, LabelDefinition) {
    auto analyzer = analyzeAndGet("start: NOP");
    auto symbol = analyzer.getSymbolTable().lookup("start");

    ASSERT_TRUE(symbol.has_value());
    EXPECT_EQ(symbol->type, SymbolType::LABEL);
    EXPECT_TRUE(symbol->is_resolved);
}

TEST_F(SemanticAnalyzerTest, MultipleLabels) {
    std::string source = R"(
        first: NOP
        second: NOP
        third: HLT
    )";

    auto analyzer = analyzeAndGet(source);

    EXPECT_TRUE(analyzer.getSymbolTable().exists("first"));
    EXPECT_TRUE(analyzer.getSymbolTable().exists("second"));
    EXPECT_TRUE(analyzer.getSymbolTable().exists("third"));
}

TEST_F(SemanticAnalyzerTest, LocalLabels) {
    std::string source = R"(
        global_func:
            NOP
        .loop:
            JMP .loop
    )";

    auto analyzer = analyzeAndGet(source);
    EXPECT_TRUE(analyzer.getSymbolTable().exists("global_func"));
}

TEST_F(SemanticAnalyzerTest, DuplicateLabelError) {
    std::string source = R"(
        start: NOP
        start: HLT
    )";

    EXPECT_FALSE(analyzeSucceeds(source));
}

TEST_F(SemanticAnalyzerTest, EQUConstant) {
    std::string source = R"(
        BUFFER_SIZE EQU 256
        RESB BUFFER_SIZE
    )";

    auto analyzer = analyzeAndGet(source);
    auto symbol = analyzer.getSymbolTable().lookup("BUFFER_SIZE");

    ASSERT_TRUE(symbol.has_value());
    EXPECT_EQ(symbol->type, SymbolType::CONSTANT);
    EXPECT_EQ(symbol->value, 256);
}

TEST_F(SemanticAnalyzerTest, DuplicateConstantError) {
    std::string source = R"(
        VALUE EQU 10
        VALUE EQU 20
    )";

    EXPECT_FALSE(analyzeSucceeds(source));
}

TEST_F(SemanticAnalyzerTest, ORGDirective) {
    std::string source = R"(
        ORG 0x7C00
        start: NOP
    )";

    auto analyzer = analyzeAndGet(source);

    EXPECT_EQ(analyzer.getOriginAddress(), 0x7C00);

    auto symbol = analyzer.getSymbolTable().lookup("start");
    ASSERT_TRUE(symbol.has_value());
    EXPECT_EQ(symbol->value, 0x7C00);
}

TEST_F(SemanticAnalyzerTest, ORGAffectsLabelAddresses) {
    std::string source = R"(
        ORG 0x1000
        first: NOP
        second: NOP
    )";

    auto analyzer = analyzeAndGet(source);

    auto first = analyzer.getSymbolTable().lookup("first");
    auto second = analyzer.getSymbolTable().lookup("second");

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());

    EXPECT_EQ(first->value, 0x1000);
    EXPECT_EQ(second->value, 0x1001);
}

TEST_F(SemanticAnalyzerTest, InstructionSizeCalculation) {
    std::string source = R"(
        first: NOP
        second: MOV AX, BX
        third: MOV AX, 0x1234
    )";

    auto analyzer = analyzeAndGet(source);

    auto first = analyzer.getSymbolTable().lookup("first");
    auto second = analyzer.getSymbolTable().lookup("second");
    auto third = analyzer.getSymbolTable().lookup("third");

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(third.has_value());

    EXPECT_EQ(first->value, 0);
    EXPECT_EQ(second->value, 1);
    EXPECT_EQ(third->value, 3);
}

TEST_F(SemanticAnalyzerTest, DataDirectiveSizes) {
    std::string source = R"(
        byte_data: DB 0
        word_data: DW 0
        dword_data: DD 0
    )";

    auto analyzer = analyzeAndGet(source);

    auto byte_label = analyzer.getSymbolTable().lookup("byte_data");
    auto word_label = analyzer.getSymbolTable().lookup("word_data");
    auto dword_label = analyzer.getSymbolTable().lookup("dword_data");

    ASSERT_TRUE(byte_label.has_value());
    ASSERT_TRUE(word_label.has_value());
    ASSERT_TRUE(dword_label.has_value());

    EXPECT_EQ(byte_label->value, 0);
    EXPECT_EQ(word_label->value, 1);
    EXPECT_EQ(dword_label->value, 3);
}

TEST_F(SemanticAnalyzerTest, StringDataSize) {
    std::string source = R"(
        msg: DB "Hello"
        after: NOP
    )";

    auto analyzer = analyzeAndGet(source);

    auto msg = analyzer.getSymbolTable().lookup("msg");
    auto after = analyzer.getSymbolTable().lookup("after");

    ASSERT_TRUE(msg.has_value());
    ASSERT_TRUE(after.has_value());

    EXPECT_EQ(msg->value, 0);
    EXPECT_EQ(after->value, 5);
}

TEST_F(SemanticAnalyzerTest, RESBDirectiveSize) {
    std::string source = R"(
        buffer: RESB 100
        after: NOP
    )";

    auto analyzer = analyzeAndGet(source);

    auto buffer = analyzer.getSymbolTable().lookup("buffer");
    auto after = analyzer.getSymbolTable().lookup("after");

    ASSERT_TRUE(buffer.has_value());
    ASSERT_TRUE(after.has_value());

    EXPECT_EQ(buffer->value, 0);
    EXPECT_EQ(after->value, 100);
}

TEST_F(SemanticAnalyzerTest, RESWDirectiveSize) {
    std::string source = R"(
        buffer: RESW 50
        after: NOP
    )";

    auto analyzer = analyzeAndGet(source);

    auto buffer = analyzer.getSymbolTable().lookup("buffer");
    auto after = analyzer.getSymbolTable().lookup("after");

    ASSERT_TRUE(buffer.has_value());
    ASSERT_TRUE(after.has_value());

    EXPECT_EQ(buffer->value, 0);
    EXPECT_EQ(after->value, 100);
}

TEST_F(SemanticAnalyzerTest, TIMESDirectiveSize) {
    std::string source = R"(
        padding: TIMES 10 DB 0
        after: NOP
    )";

    auto analyzer = analyzeAndGet(source);

    auto padding = analyzer.getSymbolTable().lookup("padding");
    auto after = analyzer.getSymbolTable().lookup("after");

    ASSERT_TRUE(padding.has_value());
    ASSERT_TRUE(after.has_value());

    EXPECT_EQ(padding->value, 0);
    EXPECT_EQ(after->value, 10);
}

TEST_F(SemanticAnalyzerTest, SegmentDirective) {
    std::string source = R"(
        SEGMENT .text
            NOP
        ENDS .text
    )";

    EXPECT_TRUE(analyzeSucceeds(source));
}

TEST_F(SemanticAnalyzerTest, MultipleSegments) {
    std::string source = R"(
        SEGMENT .text
            start: NOP
                   HLT
        ENDS .text

        SEGMENT .data
            msg: DB "Hello", 0
        ENDS .data
    )";

    auto analyzer = analyzeAndGet(source);

    EXPECT_TRUE(analyzer.getSymbolTable().exists("start"));
    EXPECT_TRUE(analyzer.getSymbolTable().exists("msg"));
}

TEST_F(SemanticAnalyzerTest, ShortJumpSize) {
    std::string source = R"(
        start: JMP SHORT .loop
        .loop: NOP
    )";

    auto analyzer = analyzeAndGet(source);
}

TEST_F(SemanticAnalyzerTest, NearJumpSize) {
    std::string source = R"(
        start: JMP target
        target: NOP
    )";

    auto analyzer = analyzeAndGet(source);

    auto start = analyzer.getSymbolTable().lookup("start");
    auto target = analyzer.getSymbolTable().lookup("target");

    ASSERT_TRUE(start.has_value());
    ASSERT_TRUE(target.has_value());

    EXPECT_EQ(target->value - start->value, 3);
}

TEST_F(SemanticAnalyzerTest, ConditionalJumpSize) {
    std::string source = R"(
        start: JE target
        target: NOP
    )";

    auto analyzer = analyzeAndGet(source);

    auto start = analyzer.getSymbolTable().lookup("start");
    auto target = analyzer.getSymbolTable().lookup("target");

    ASSERT_TRUE(start.has_value());
    ASSERT_TRUE(target.has_value());

    EXPECT_EQ(target->value - start->value, 2);
}

TEST_F(SemanticAnalyzerTest, PushPopSize) {
    std::string source = R"(
        start: PUSH AX
        mid: POP BX
        end_: NOP
    )";

    auto analyzer = analyzeAndGet(source);

    auto start = analyzer.getSymbolTable().lookup("start");
    auto mid = analyzer.getSymbolTable().lookup("mid");
    auto end = analyzer.getSymbolTable().lookup("end_");

    ASSERT_TRUE(start.has_value());
    ASSERT_TRUE(mid.has_value());
    ASSERT_TRUE(end.has_value());

    EXPECT_EQ(mid->value - start->value, 1);
    EXPECT_EQ(end->value - mid->value, 1);
}

TEST_F(SemanticAnalyzerTest, IncDecSize) {
    std::string source = R"(
        start: INC AX
        mid: DEC BX
        end_: NOP
    )";

    auto analyzer = analyzeAndGet(source);

    auto start = analyzer.getSymbolTable().lookup("start");
    auto mid = analyzer.getSymbolTable().lookup("mid");

    ASSERT_TRUE(start.has_value());
    ASSERT_TRUE(mid.has_value());

    EXPECT_EQ(mid->value - start->value, 1);
}

TEST_F(SemanticAnalyzerTest, IntInstructionSize) {
    std::string source = R"(
        start: INT 0x21
        end_: NOP
    )";

    auto analyzer = analyzeAndGet(source);

    auto start = analyzer.getSymbolTable().lookup("start");
    auto end = analyzer.getSymbolTable().lookup("end_");

    ASSERT_TRUE(start.has_value());
    ASSERT_TRUE(end.has_value());

    EXPECT_EQ(end->value - start->value, 2);
}

TEST_F(SemanticAnalyzerTest, BootloaderStructure) {
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

        .loop:
            HLT
            JMP .loop

        TIMES 510-($-$$) DB 0
        DW 0xAA55
    )";

    auto analyzer = analyzeAndGet(source);

    EXPECT_EQ(analyzer.getOriginAddress(), 0x7C00);
    EXPECT_TRUE(analyzer.getSymbolTable().exists("start"));
    EXPECT_TRUE(analyzer.getErrors().empty());
}

TEST_F(SemanticAnalyzerTest, GetAddressByStatementIndex) {
    std::string source = R"(
        NOP
        NOP
        NOP
    )";

    auto program = parse(source);
    SemanticAnalyzer analyzer;
    analyzer.analyze(program.get());

    auto addr0 = analyzer.getAddress(0);
    auto addr1 = analyzer.getAddress(1);
    auto addr2 = analyzer.getAddress(2);

    ASSERT_TRUE(addr0.has_value());
    ASSERT_TRUE(addr1.has_value());
    ASSERT_TRUE(addr2.has_value());

    EXPECT_EQ(*addr0, 0);
    EXPECT_EQ(*addr1, 1);
    EXPECT_EQ(*addr2, 2);
}

class SymbolTableTest : public ::testing::Test {
protected:
    SymbolTable table;
};

TEST_F(SymbolTableTest, DefineAndLookup) {
    EXPECT_TRUE(table.define("test", SymbolType::LABEL, 100, 1));

    auto symbol = table.lookup("test");
    ASSERT_TRUE(symbol.has_value());
    EXPECT_EQ(symbol->name, "test");
    EXPECT_EQ(symbol->value, 100);
    EXPECT_EQ(symbol->type, SymbolType::LABEL);
}

TEST_F(SymbolTableTest, DuplicateDefinitionFails) {
    EXPECT_TRUE(table.define("test", SymbolType::LABEL, 100, 1));
    EXPECT_FALSE(table.define("test", SymbolType::LABEL, 200, 2));
}

TEST_F(SymbolTableTest, CaseInsensitiveLookup) {
    table.define("MyLabel", SymbolType::LABEL, 100, 1);

    auto upper = table.lookup("MYLABEL");
    auto lower = table.lookup("mylabel");
    auto mixed = table.lookup("MyLabel");

    EXPECT_TRUE(upper.has_value());
    EXPECT_TRUE(lower.has_value());
    EXPECT_TRUE(mixed.has_value());
}

TEST_F(SymbolTableTest, UpdateSymbol) {
    table.define("test", SymbolType::LABEL, 100, 1);
    EXPECT_TRUE(table.update("test", 200));

    auto symbol = table.lookup("test");
    ASSERT_TRUE(symbol.has_value());
    EXPECT_EQ(symbol->value, 200);
}

TEST_F(SymbolTableTest, UpdateNonexistentFails) {
    EXPECT_FALSE(table.update("nonexistent", 100));
}

TEST_F(SymbolTableTest, ExistsCheck) {
    EXPECT_FALSE(table.exists("test"));
    table.define("test", SymbolType::LABEL, 100, 1);
    EXPECT_TRUE(table.exists("test"));
}

TEST_F(SymbolTableTest, LocalLabelDetection) {
    EXPECT_TRUE(SymbolTable::isLocalLabel(".local"));
    EXPECT_TRUE(SymbolTable::isLocalLabel(".loop"));
    EXPECT_FALSE(SymbolTable::isLocalLabel("global"));
    EXPECT_FALSE(SymbolTable::isLocalLabel("_start"));
}

TEST_F(SymbolTableTest, GlobalScopeManagement) {
    table.setGlobalScope("main");
    EXPECT_EQ(table.getGlobalScope(), "main");

    table.setGlobalScope("other");
    EXPECT_EQ(table.getGlobalScope(), "other");
}

TEST_F(SymbolTableTest, FullyQualifiedName) {
    table.setGlobalScope("main");

    EXPECT_EQ(table.getFullyQualifiedName(".local"), "main.local");
    EXPECT_EQ(table.getFullyQualifiedName("global"), "global");
}

TEST_F(SymbolTableTest, FullyQualifiedNameNoScope) {
    EXPECT_EQ(table.getFullyQualifiedName(".local"), ".local");
    EXPECT_EQ(table.getFullyQualifiedName("global"), "global");
}

TEST_F(SymbolTableTest, ClearTable) {
    table.define("test1", SymbolType::LABEL, 100, 1);
    table.define("test2", SymbolType::LABEL, 200, 2);
    table.setGlobalScope("main");

    table.clear();

    EXPECT_FALSE(table.exists("test1"));
    EXPECT_FALSE(table.exists("test2"));
    EXPECT_EQ(table.getGlobalScope(), "");
}

TEST_F(SymbolTableTest, GetAllSymbols) {
    table.define("a", SymbolType::LABEL, 100, 1);
    table.define("b", SymbolType::CONSTANT, 200, 2);
    table.define("c", SymbolType::LABEL, 300, 3);

    auto& all = table.getAllSymbols();
    EXPECT_EQ(all.size(), 3);
}

TEST_F(SymbolTableTest, ConstantType) {
    table.define("BUFFER_SIZE", SymbolType::CONSTANT, 1024, 1);

    auto symbol = table.lookup("BUFFER_SIZE");
    ASSERT_TRUE(symbol.has_value());
    EXPECT_EQ(symbol->type, SymbolType::CONSTANT);
    EXPECT_EQ(symbol->value, 1024);
}
