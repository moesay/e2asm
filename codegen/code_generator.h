#pragma once

#include <vector>
#include <cstdint>
#include "../parser/ast.h"
#include "../core/assembler.h"
#include "../core/error.h"
#include "../semantic/semantic_analyzer.h"
#include "instruction_encoder.h"

namespace e2asm {

class CodeGenerator {
public:
    CodeGenerator();

    AssemblyResult generate(const Program* program);

private:
    bool generateStatement(const ASTNode* stmt);
    void processLabel(const Label* label);
    bool processInstruction(const Instruction* instr);
    bool processDataDirective(const DataDirective* directive);
    void processEQUDirective(const EQUDirective* directive);
    void processORGDirective(const ORGDirective* directive);
    void processSEGMENTDirective(const SEGMENTDirective* directive);
    void processENDSDirective(const ENDSDirective* directive);
    bool processRESDirective(const RESDirective* directive);
    bool processTIMESDirective(const TIMESDirective* directive);

    SemanticAnalyzer m_semantic_analyzer;
    InstructionEncoder m_encoder;
    std::vector<uint8_t> m_binary;
    std::vector<AssembledLine> m_listing;
    ErrorReporter m_error_reporter;
    size_t m_current_address;
};

} // namespace e2asm
