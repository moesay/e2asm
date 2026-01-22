#pragma once

#include "symbol_table.h"
#include "../parser/ast.h"
#include "../core/error.h"
#include <vector>
#include <memory>

namespace e2asm {

struct AddressInfo {
    size_t statement_index;
    uint64_t address;
    uint64_t size;  // Size in bytes (may change in the future for optimization)
};

/**
 * Semantic analyzer performs multi-pass assembly:
 * - Pass 1: Build symbol table, assign temp addresses
 * - Pass 2: Resolve forward references, iterate until stable
 * - TODO: jmp distance optimization (short jmps)
 */
class SemanticAnalyzer {
public:
    SemanticAnalyzer();

    bool analyze(Program* program);

    const SymbolTable& getSymbolTable() const { return m_symbol_table; }
    SymbolTable& getSymbolTable() { return m_symbol_table; }

    std::optional<uint64_t> getAddress(size_t statement_index) const;

    uint64_t getOriginAddress() const { return m_origin_address; }

    const std::vector<Error>& getErrors() const { return m_errors; }

    void clear();

private:
     // Pass 1: Build symbol table and assign temporary addresses
    bool pass1_buildSymbols(Program* program);

    /**
     * Pass 2: Resolve symbols and calculate final addresses
     * Returns true if the temp addresses changed (need another iteration)
     */
    bool pass2_resolveSymbols(Program* program);

    uint64_t calculateInstructionSize(Instruction* instr);

    uint64_t calculateDataSize(const std::string& directive, size_t value_count);

    void error(const std::string& message, SourceLocation loc);

    // For segment tracking
    struct SegmentInfo {
        std::string name;
        uint64_t start_address;
        uint64_t current_address;
    };

    void setOrigin(uint64_t address);
    void enterSegment(const std::string& name);
    void exitSegment(const std::string& name);

    SymbolTable m_symbol_table;
    std::vector<AddressInfo> m_addresses;
    std::vector<Error> m_errors;
    uint64_t m_current_address;

    std::vector<SegmentInfo> m_segments;
    std::string m_current_segment;
    uint64_t m_segment_start_address;  // For $$ marker
    uint64_t m_origin_address;         // Base address from ORG
    // To prevent falling from a section to another without an instruction
    bool m_last_was_terminator;

    bool isCodeSegment(const std::string& name) const;

    bool isDataSegment(const std::string& name) const;
};

} // namespace e2asm
