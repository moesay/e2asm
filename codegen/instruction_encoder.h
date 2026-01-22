#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include "instruction_tables.h"
#include "../parser/ast.h"
#include "../core/error.h"
#include "../semantic/symbol_table.h"

namespace e2asm {

/**
 * Encoded instruction result
 */
struct EncodedInstruction {
    std::vector<uint8_t> bytes;      // Generated machine code
    bool success;                     // True if encoding succeeded
    std::string error;                // Error message if failed

    EncodedInstruction() : success(false) {}
    EncodedInstruction(std::vector<uint8_t> b) : bytes(std::move(b)), success(true) {}
    EncodedInstruction(std::string err) : success(false), error(std::move(err)) {}
};

/**
 * Instruction encoder - converts AST instructions to machine code
 * Uses table-driven approach instead of per-instruction classes
 */
class InstructionEncoder {
public:
    InstructionEncoder();

    /**
     * Set symbol table for label resolution
     */
    void setSymbolTable(const SymbolTable* symbols) { m_symbol_table = symbols; }

    /**
     * Set current address for relative jump calculation
     */
    void setCurrentAddress(uint64_t address) { m_current_address = address; }

    /**
     * Encode a single instruction to machine code
     * @param instr Instruction AST node
     * @return Encoded bytes or error
     */
    EncodedInstruction encode(const Instruction* instr);

private:
    /**
     * Find matching encoding for instruction + operands
     */
    const InstructionEncoding* findEncoding(
        const std::string& mnemonic,
        const std::vector<std::unique_ptr<Operand>>& operands
    );

    /**
     * Check if operand matches the expected spec
     */
    bool matchesSpec(const Operand* operand, OperandSpec spec);

    /**
     * Classify operand into OperandSpec
     */
    OperandSpec classifyOperand(const Operand* operand);

    /**
     * Encode using different encoding types
     */
    EncodedInstruction encodeModRM(
        const InstructionEncoding* encoding,
        const Instruction* instr
    );

    EncodedInstruction encodeRegInOpcode(
        const InstructionEncoding* encoding,
        const Instruction* instr
    );

    EncodedInstruction encodeImmediate(
        const InstructionEncoding* encoding,
        const Instruction* instr
    );

    EncodedInstruction encodeModRMImm(
        const InstructionEncoding* encoding,
        const Instruction* instr
    );

    EncodedInstruction encodeRelative(
        const InstructionEncoding* encoding,
        const Instruction* instr
    );

    /**
     * Generate ModR/M byte
     * @param mod MOD field (0-3)
     * @param reg REG field (0-7)
     * @param rm R/M field (0-7)
     */
    uint8_t generateModRM(uint8_t mod, uint8_t reg, uint8_t rm);

    /**
     * Encode immediate value as bytes (little-endian)
     */
    std::vector<uint8_t> encodeImmediate(int64_t value, size_t size_bytes);

    /**
     * Check if operand is accumulator (AL or AX)
     */
    bool isAccumulator(const Operand* operand);

    /**
     * Lookup label with fallback for segment names
     * Tries normal lookup first, then direct lookup for labels starting with '.'
     */
    std::optional<Symbol> lookupLabel(const std::string& label_name) const;

    /**
     * Get segment override prefix byte for a segment register
     * @param segment Segment register name ("ES", "CS", "SS", "DS")
     * @return Prefix byte (0x26, 0x2E, 0x36, 0x3E) or nullopt if invalid
     */
    std::optional<uint8_t> getSegmentOverridePrefix(const std::string& segment) const;

    /**
     * Evaluate expression by substituting EQU constants
     * @param expr Expression string (e.g., "WIDTH - RECT_W")
     * @return Evaluated value or nullopt on error
     */
    std::optional<int64_t> evaluateExpression(const std::string& expr) const;

    const SymbolTable* m_symbol_table = nullptr;
    uint64_t m_current_address = 0;
};

} // namespace e2asm
