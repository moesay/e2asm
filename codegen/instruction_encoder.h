/**
 * @file instruction_encoder.h
 * @brief Converts AST instructions into 8086 machine code bytes
 *
 * The instruction encoder handles the complex task of translating assembly
 * instructions into their binary encodings. The 8086 has many encoding variants
 * (ModR/M, register-in-opcode, immediate, relative) which this module navigates
 * using a table-driven approach.
 */

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
 * @brief Result of encoding a single instruction
 *
 * Either contains the generated machine code bytes or an error message
 * explaining why encoding failed. Success can be checked via the success flag.
 */
struct EncodedInstruction {
    std::vector<uint8_t> bytes;  ///< Machine code bytes (if successful)
    bool success;                ///< True if encoding succeeded
    std::string error;           ///< Error message (if failed)

    EncodedInstruction() : success(false) {}
    EncodedInstruction(std::vector<uint8_t> b) : bytes(std::move(b)), success(true) {}
    EncodedInstruction(std::string err) : success(false), error(std::move(err)) {}
};

/**
 * @brief Table-driven 8086 instruction encoder
 *
 * Converts parsed instructions into machine code using encoding tables rather
 * than per-instruction logic. This makes the encoder data-driven and easier
 * to extend with new instructions.
 *
 * The 8086 has several encoding forms:
 *
 * **ModR/M Encoding**: Most instructions use a ModR/M byte to specify operands
 * - MOD field (2 bits): Addressing mode (register, memory, displacement size)
 * - REG field (3 bits): Register or opcode extension
 * - R/M field (3 bits): Register or memory operand
 *
 * **Register-in-Opcode**: Register encoded in opcode byte itself
 * - INC AX → 0x40 (AX's code 0 added to base 0x40)
 * - INC CX → 0x41 (CX's code 1 added to base 0x40)
 *
 * **Immediate**: Constant value follows opcode
 * - MOV AX, 1234 → B8 D2 04 (word immediate in little-endian)
 *
 * **Relative**: Offset for jumps/calls
 * - JMP label → E9 XX XX (signed offset to target)
 *
 * The encoder:
 * 1. Finds matching encoding from tables based on mnemonic and operand types
 * 2. Generates opcode byte(s)
 * 3. Constructs ModR/M and displacement bytes if needed
 * 4. Appends immediate values or offsets
 * 5. Handles segment overrides and size prefixes
 */
class InstructionEncoder {
public:
    InstructionEncoder();

    /**
     * @brief Provides symbol table for resolving label references
     * @param symbols Symbol table with all defined labels and constants
     *
     * Must be called before encoding instructions that reference symbols.
     */
    void setSymbolTable(const SymbolTable* symbols) { m_symbol_table = symbols; }

    /**
     * @brief Sets current assembly address for relative jumps
     * @param address Current position in output
     *
     * Relative jumps are calculated as (target - current - instruction_size).
     * This address should be where the instruction will be placed.
     */
    void setCurrentAddress(uint64_t address) { m_current_address = address; }

    /**
     * @brief Encodes an instruction to machine code
     * @param instr Instruction AST node with mnemonic and operands
     * @return Encoded bytes if successful, or error details
     *
     * This is the main entry point. Handles all encoding forms automatically
     * by consulting the instruction tables.
     */
    EncodedInstruction encode(const Instruction* instr);

private:
    /**
     * @brief Finds the encoding table entry for an instruction
     * @param mnemonic Instruction name (MOV, ADD, etc.)
     * @param operands Instruction's operands
     * @return Pointer to encoding info, or nullptr if not found
     *
     * Searches the instruction tables for a row matching both the mnemonic
     * and operand pattern. Some instructions have multiple encodings
     * (e.g., MOV has separate forms for reg-to-reg, reg-to-mem, immediate).
     */
    const InstructionEncoding* findEncoding(
        const std::string& mnemonic,
        const std::vector<std::unique_ptr<Operand>>& operands
    );

    /**
     * @brief Checks if an operand matches expected specification
     * @param operand Actual operand from instruction
     * @param spec Expected pattern from encoding table
     * @return true if operand fits the specification
     */
    bool matchesSpec(const Operand* operand, OperandSpec spec);

    /**
     * @brief Classifies an operand into a specification category
     * @param operand Operand to classify
     * @return Specification enum describing the operand type
     *
     * Used to match operands against encoding table patterns.
     */
    OperandSpec classifyOperand(const Operand* operand);

    /**
     * @brief Encodes instruction using ModR/M byte
     * @param encoding Table entry describing how to encode
     * @param instr Instruction to encode
     * @return Machine code bytes or error
     */
    EncodedInstruction encodeModRM(
        const InstructionEncoding* encoding,
        const Instruction* instr
    );

    /**
     * @brief Encodes instruction with register in opcode
     * @param encoding Table entry describing how to encode
     * @param instr Instruction to encode
     * @return Machine code bytes or error
     */
    EncodedInstruction encodeRegInOpcode(
        const InstructionEncoding* encoding,
        const Instruction* instr
    );

    /**
     * @brief Encodes instruction with immediate value
     * @param encoding Table entry describing how to encode
     * @param instr Instruction to encode
     * @return Machine code bytes or error
     */
    EncodedInstruction encodeImmediate(
        const InstructionEncoding* encoding,
        const Instruction* instr
    );

    /**
     * @brief Encodes instruction with ModR/M and immediate
     * @param encoding Table entry describing how to encode
     * @param instr Instruction to encode
     * @return Machine code bytes or error
     */
    EncodedInstruction encodeModRMImm(
        const InstructionEncoding* encoding,
        const Instruction* instr
    );

    /**
     * @brief Encodes relative jump or call
     * @param encoding Table entry describing how to encode
     * @param instr Instruction to encode
     * @return Machine code bytes or error
     */
    EncodedInstruction encodeRelative(
        const InstructionEncoding* encoding,
        const Instruction* instr
    );

    /**
     * @brief Constructs an 8086 ModR/M byte
     * @param mod MOD field (0-3): addressing mode
     * @param reg REG field (0-7): register or opcode extension
     * @param rm R/M field (0-7): register or memory operand
     * @return Encoded ModR/M byte
     *
     * ModR/M byte format: [mod:2][reg:3][rm:3]
     */
    uint8_t generateModRM(uint8_t mod, uint8_t reg, uint8_t rm);

    /**
     * @brief Converts immediate value to little-endian bytes
     * @param value Numeric value to encode
     * @param size_bytes Number of bytes (1, 2, or 4)
     * @return Vector of bytes in little-endian order
     */
    std::vector<uint8_t> encodeImmediate(int64_t value, size_t size_bytes);

    /**
     * @brief Checks if operand is the accumulator register
     * @param operand Operand to check
     * @return true for AL or AX
     *
     * Some instructions have special short encodings when the accumulator is used.
     */
    bool isAccumulator(const Operand* operand);

    /**
     * @brief Looks up a symbol with scoping fallback
     * @param label_name Symbol to find
     * @return Symbol if found
     *
     * Tries normal scoped lookup first, then direct lookup for local labels.
     */
    std::optional<Symbol> lookupLabel(const std::string& label_name) const;

    /**
     * @brief Gets segment override prefix byte
     * @param segment Segment register name ("ES", "CS", "SS", "DS")
     * @return Prefix byte or nullopt if invalid
     *
     * Segment overrides are single-byte prefixes before the instruction.
     */
    std::optional<uint8_t> getSegmentOverridePrefix(const std::string& segment) const;

    /**
     * @brief Evaluates arithmetic expression with EQU constants
     * @param expr Expression string (e.g., "WIDTH - RECT_W")
     * @return Computed value or nullopt on error
     *
     * Substitutes EQU constant names with their values and evaluates.
     */
    std::optional<int64_t> evaluateExpression(const std::string& expr) const;

    const SymbolTable* m_symbol_table = nullptr;  ///< For resolving labels
    uint64_t m_current_address = 0;               ///< For calculating relative jumps
};

} // namespace e2asm
