#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace e2asm {

/**
 * Operand specification for instruction encoding
 */
enum class OperandSpec {
    NONE,           // No operand

    // General purpose registers
    REG8,           // Any 8-bit register (AL, BL, CL, DL, AH, BH, CH, DH)
    REG16,          // Any 16-bit register (AX, BX, CX, DX, SP, BP, SI, DI)

    // Memory operands
    MEM8,           // 8-bit memory operand
    MEM16,          // 16-bit memory operand

    // Register or memory (used with ModRM)
    RM8,            // Register or memory 8-bit
    RM16,           // Register or memory 16-bit

    // Immediates
    IMM8,           // 8-bit immediate value
    IMM16,          // 16-bit immediate value

    // Specific registers (for special encodings)
    AL,             // Accumulator low (8-bit)
    AX,             // Accumulator (16-bit)
    CL,             // Count register
    DX,             // Data register

    // Segment registers
    SEGREG,         // Segment register (ES, CS, SS, DS)

    // For jumps and calls
    REL8,           // 8-bit relative offset (short jump)
    REL16,          // 16-bit relative offset (near jump)

    // Labels
    LABEL,          // Label reference
};

// How the instruction is encoded

enum class EncodingType {
    // Opcode + ModR/M byte
    // Format: [opcode] [ModR/M] [displacement] [immediate]
    MODRM,

    // Fixed opcode only (no operands)
    // Format: [opcode]
    FIXED,

    // Register encoded in opcode (e.g., MOV AX, imm16 = B8 + reg)
    // Format: [opcode+reg] [immediate]
    REG_IN_OPCODE,

    // Opcode + immediate
    // Format: [opcode] [immediate]
    IMMEDIATE,

    // Opcode + ModR/M + immediate (for instructions like ADD r/m, imm)
    // Format: [opcode] [ModR/M] [displacement] [immediate]
    MODRM_IMM,

    // Relative jump
    // Format: [opcode] [rel8/rel16]
    RELATIVE,
};

/**
 * Single instruction encoding variant
 * One instruction (like MOV) has multiple encodings for different operand combinations
 */
struct InstructionEncoding {
    std::string mnemonic;                   // "MOV", "ADD", etc.
    std::vector<OperandSpec> operands;      // Expected operand types
    EncodingType encoding_type;             // How to encode this variant
    uint8_t base_opcode;                    // Base opcode byte
    uint8_t modrm_reg_field;                // For MODRM_IMM: value for reg field (e.g., ADD uses /0)
    bool has_direction_bit;                 // D bit: 0=reg is source, 1=reg is dest
    bool has_width_bit;                     // W bit: 0=8-bit, 1=16-bit

    InstructionEncoding(
        std::string mn,
        std::vector<OperandSpec> ops,
        EncodingType enc,
        uint8_t opcode,
        uint8_t reg_field = 0,
        bool d_bit = false,
        bool w_bit = false
    )
        : mnemonic(std::move(mn))
        , operands(std::move(ops))
        , encoding_type(enc)
        , base_opcode(opcode)
        , modrm_reg_field(reg_field)
        , has_direction_bit(d_bit)
        , has_width_bit(w_bit)
    {}
};

/**
  Master instructions table
  This should be the only source of truth for instructions encoding in the porject
  but for now, this is not the case as the instructions size is being calculated
  elsewhere, this should be fixed.
*/
inline const std::vector<InstructionEncoding> INSTRUCTION_TABLE = {
    // ========== MOV ==========
    // Register to register/memory (opcode 0x88/0x89)
    {"MOV", {OperandSpec::RM8, OperandSpec::REG8}, EncodingType::MODRM, 0x88},
    {"MOV", {OperandSpec::RM16, OperandSpec::REG16}, EncodingType::MODRM, 0x89},

    // Register/memory to register (opcode 0x8A/0x8B)
    {"MOV", {OperandSpec::REG8, OperandSpec::RM8}, EncodingType::MODRM, 0x8A},
    {"MOV", {OperandSpec::REG16, OperandSpec::RM16}, EncodingType::MODRM, 0x8B},

    // Immediate to register/memory (opcode 0xC6/0xC7)
    {"MOV", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xC6, 0},
    {"MOV", {OperandSpec::RM16, OperandSpec::IMM16}, EncodingType::MODRM_IMM, 0xC7, 0},

    // Accumulator to/from memory (special opcodes)
    {"MOV", {OperandSpec::AL, OperandSpec::MEM8}, EncodingType::IMMEDIATE, 0xA0},
    {"MOV", {OperandSpec::AX, OperandSpec::MEM16}, EncodingType::IMMEDIATE, 0xA1},
    {"MOV", {OperandSpec::MEM8, OperandSpec::AL}, EncodingType::IMMEDIATE, 0xA2},
    {"MOV", {OperandSpec::MEM16, OperandSpec::AX}, EncodingType::IMMEDIATE, 0xA3},

    // Immediate to register (B0-B7 for 8-bit, B8-BF for 16-bit)
    {"MOV", {OperandSpec::AL, OperandSpec::IMM8}, EncodingType::REG_IN_OPCODE, 0xB0},
    {"MOV", {OperandSpec::REG8, OperandSpec::IMM8}, EncodingType::REG_IN_OPCODE, 0xB0},
    {"MOV", {OperandSpec::AX, OperandSpec::IMM16}, EncodingType::REG_IN_OPCODE, 0xB8},
    {"MOV", {OperandSpec::REG16, OperandSpec::IMM16}, EncodingType::REG_IN_OPCODE, 0xB8},

    // Segment register moves
    {"MOV", {OperandSpec::RM16, OperandSpec::SEGREG}, EncodingType::MODRM, 0x8C},
    {"MOV", {OperandSpec::SEGREG, OperandSpec::RM16}, EncodingType::MODRM, 0x8E},

    // ========== ADD ==========
    // Register to register/memory (opcode 0x00/0x01)
    {"ADD", {OperandSpec::RM8, OperandSpec::REG8}, EncodingType::MODRM, 0x00},
    {"ADD", {OperandSpec::RM16, OperandSpec::REG16}, EncodingType::MODRM, 0x01},

    // Register/memory to register (opcode 0x02/0x03)
    {"ADD", {OperandSpec::REG8, OperandSpec::RM8}, EncodingType::MODRM, 0x02},
    {"ADD", {OperandSpec::REG16, OperandSpec::RM16}, EncodingType::MODRM, 0x03},

    // Immediate to accumulator (opcode 0x04/0x05)
    {"ADD", {OperandSpec::AL, OperandSpec::IMM8}, EncodingType::IMMEDIATE, 0x04},
    {"ADD", {OperandSpec::AX, OperandSpec::IMM16}, EncodingType::IMMEDIATE, 0x05},

    // Immediate to register/memory (opcode 0x80/0x81 with /0 in reg field)
    {"ADD", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x80, 0},
    {"ADD", {OperandSpec::RM16, OperandSpec::IMM16}, EncodingType::MODRM_IMM, 0x81, 0},
    {"ADD", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x83, 0},  // Sign-extended

    // ========== ADC ==========
    // Register to register/memory (opcode 0x10/0x11)
    {"ADC", {OperandSpec::RM8, OperandSpec::REG8}, EncodingType::MODRM, 0x10},
    {"ADC", {OperandSpec::RM16, OperandSpec::REG16}, EncodingType::MODRM, 0x11},

    // Register/memory to register (opcode 0x12/0x13)
    {"ADC", {OperandSpec::REG8, OperandSpec::RM8}, EncodingType::MODRM, 0x12},
    {"ADC", {OperandSpec::REG16, OperandSpec::RM16}, EncodingType::MODRM, 0x13},

    // Immediate to accumulator (opcode 0x14/0x15)
    {"ADC", {OperandSpec::AL, OperandSpec::IMM8}, EncodingType::IMMEDIATE, 0x14},
    {"ADC", {OperandSpec::AX, OperandSpec::IMM16}, EncodingType::IMMEDIATE, 0x15},

    // Immediate to register/memory (opcode 0x80/0x81 with /2 in reg field)
    {"ADC", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x80, 2},
    {"ADC", {OperandSpec::RM16, OperandSpec::IMM16}, EncodingType::MODRM_IMM, 0x81, 2},
    {"ADC", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x83, 2},  // Sign-extended

    // ========== SUB ==========
    // Register to register/memory (opcode 0x28/0x29)
    {"SUB", {OperandSpec::RM8, OperandSpec::REG8}, EncodingType::MODRM, 0x28},
    {"SUB", {OperandSpec::RM16, OperandSpec::REG16}, EncodingType::MODRM, 0x29},

    // Register/memory to register (opcode 0x2A/0x2B)
    {"SUB", {OperandSpec::REG8, OperandSpec::RM8}, EncodingType::MODRM, 0x2A},
    {"SUB", {OperandSpec::REG16, OperandSpec::RM16}, EncodingType::MODRM, 0x2B},

    // Immediate to accumulator (opcode 0x2C/0x2D)
    {"SUB", {OperandSpec::AL, OperandSpec::IMM8}, EncodingType::IMMEDIATE, 0x2C},
    {"SUB", {OperandSpec::AX, OperandSpec::IMM16}, EncodingType::IMMEDIATE, 0x2D},

    // Immediate to register/memory (opcode 0x80/0x81 with /5 in reg field)
    {"SUB", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x80, 5},
    {"SUB", {OperandSpec::RM16, OperandSpec::IMM16}, EncodingType::MODRM_IMM, 0x81, 5},
    {"SUB", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x83, 5},  // Sign-extended

    // ========== SBB ==========
    // Register to register/memory (opcode 0x18/0x19)
    {"SBB", {OperandSpec::RM8, OperandSpec::REG8}, EncodingType::MODRM, 0x18},
    {"SBB", {OperandSpec::RM16, OperandSpec::REG16}, EncodingType::MODRM, 0x19},

    // Register/memory to register (opcode 0x1A/0x1B)
    {"SBB", {OperandSpec::REG8, OperandSpec::RM8}, EncodingType::MODRM, 0x1A},
    {"SBB", {OperandSpec::REG16, OperandSpec::RM16}, EncodingType::MODRM, 0x1B},

    // Immediate to accumulator (opcode 0x1C/0x1D)
    {"SBB", {OperandSpec::AL, OperandSpec::IMM8}, EncodingType::IMMEDIATE, 0x1C},
    {"SBB", {OperandSpec::AX, OperandSpec::IMM16}, EncodingType::IMMEDIATE, 0x1D},

    // Immediate to register/memory (opcode 0x80/0x81 with /3 in reg field)
    {"SBB", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x80, 3},
    {"SBB", {OperandSpec::RM16, OperandSpec::IMM16}, EncodingType::MODRM_IMM, 0x81, 3},
    {"SBB", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x83, 3},  // Sign-extended

    // ========== JMP ==========
    // Unconditional jump
    {"JMP", {OperandSpec::REL8}, EncodingType::RELATIVE, 0xEB},   // SHORT jump
    {"JMP", {OperandSpec::REL16}, EncodingType::RELATIVE, 0xE9},  // NEAR jump

    // ========== Conditional Jumps ==========
    // All conditional jumps are SHORT only (rel8)
    {"JO", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x70},    // Jump if overflow
    {"JNO", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x71},   // Jump if not overflow
    {"JB", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x72},    // Jump if below (unsigned)
    {"JC", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x72},    // Jump if carry
    {"JNAE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x72},  // Jump if not above or equal
    {"JNB", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x73},   // Jump if not below
    {"JAE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x73},   // Jump if above or equal
    {"JNC", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x73},   // Jump if not carry
    {"JE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x74},    // Jump if equal
    {"JZ", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x74},    // Jump if zero
    {"JNE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x75},   // Jump if not equal
    {"JNZ", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x75},   // Jump if not zero
    {"JBE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x76},   // Jump if below or equal
    {"JNA", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x76},   // Jump if not above
    {"JNBE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x77},  // Jump if not below or equal
    {"JA", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x77},    // Jump if above
    {"JS", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x78},    // Jump if sign
    {"JNS", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x79},   // Jump if not sign
    {"JP", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x7A},    // Jump if parity
    {"JPE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x7A},   // Jump if parity even
    {"JNP", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x7B},   // Jump if not parity
    {"JPO", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x7B},   // Jump if parity odd
    {"JL", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x7C},    // Jump if less (signed)
    {"JNGE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x7C},  // Jump if not greater or equal
    {"JNL", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x7D},   // Jump if not less
    {"JGE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x7D},   // Jump if greater or equal
    {"JLE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x7E},   // Jump if less or equal
    {"JNG", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x7E},   // Jump if not greater
    {"JNLE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x7F},  // Jump if not less or equal
    {"JG", {OperandSpec::REL8}, EncodingType::RELATIVE, 0x7F},    // Jump if greater

    // ========== CMP ==========
    // Register to register/memory (opcode 0x38/0x39)
    {"CMP", {OperandSpec::RM8, OperandSpec::REG8}, EncodingType::MODRM, 0x38},
    {"CMP", {OperandSpec::RM16, OperandSpec::REG16}, EncodingType::MODRM, 0x39},

    // Register/memory to register (opcode 0x3A/0x3B)
    {"CMP", {OperandSpec::REG8, OperandSpec::RM8}, EncodingType::MODRM, 0x3A},
    {"CMP", {OperandSpec::REG16, OperandSpec::RM16}, EncodingType::MODRM, 0x3B},

    // Immediate to accumulator (opcode 0x3C/0x3D)
    {"CMP", {OperandSpec::AL, OperandSpec::IMM8}, EncodingType::IMMEDIATE, 0x3C},
    {"CMP", {OperandSpec::AX, OperandSpec::IMM16}, EncodingType::IMMEDIATE, 0x3D},

    // Immediate to register/memory (opcode 0x80/0x81 with /7 in reg field)
    {"CMP", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x80, 7},
    {"CMP", {OperandSpec::RM16, OperandSpec::IMM16}, EncodingType::MODRM_IMM, 0x81, 7},
    {"CMP", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x83, 7},  // Sign-extended

    // ========== INC ==========
    // General form (opcode 0xFE/0xFF with /0 in reg field)
    {"INC", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xFE, 0},
    {"INC", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xFF, 0},

    // Short form for 16-bit registers (0x40-0x47)
    {"INC", {OperandSpec::AX}, EncodingType::FIXED, 0x40},
    {"INC", {OperandSpec::REG16}, EncodingType::REG_IN_OPCODE, 0x40},

    // ========== DEC ==========
    // General form (opcode 0xFE/0xFF with /1 in reg field)
    {"DEC", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xFE, 1},
    {"DEC", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xFF, 1},

    // Short form for 16-bit registers (0x48-0x4F)
    {"DEC", {OperandSpec::AX}, EncodingType::FIXED, 0x48},
    {"DEC", {OperandSpec::REG16}, EncodingType::REG_IN_OPCODE, 0x48},

    // ========== NEG ==========
    {"NEG", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xF6, 3},
    {"NEG", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xF7, 3},

    // ========== MUL ==========
    {"MUL", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xF6, 4},
    {"MUL", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xF7, 4},

    // ========== IMUL ==========
    {"IMUL", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xF6, 5},
    {"IMUL", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xF7, 5},

    // ========== DIV ==========
    {"DIV", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xF6, 6},
    {"DIV", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xF7, 6},

    // ========== IDIV ==========
    {"IDIV", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xF6, 7},
    {"IDIV", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xF7, 7},

    // ========== AND ==========
    {"AND", {OperandSpec::RM8, OperandSpec::REG8}, EncodingType::MODRM, 0x20},
    {"AND", {OperandSpec::RM16, OperandSpec::REG16}, EncodingType::MODRM, 0x21},
    {"AND", {OperandSpec::REG8, OperandSpec::RM8}, EncodingType::MODRM, 0x22},
    {"AND", {OperandSpec::REG16, OperandSpec::RM16}, EncodingType::MODRM, 0x23},
    {"AND", {OperandSpec::AL, OperandSpec::IMM8}, EncodingType::IMMEDIATE, 0x24},
    {"AND", {OperandSpec::AX, OperandSpec::IMM16}, EncodingType::IMMEDIATE, 0x25},
    {"AND", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x80, 4},
    {"AND", {OperandSpec::RM16, OperandSpec::IMM16}, EncodingType::MODRM_IMM, 0x81, 4},
    {"AND", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x83, 4},

    // ========== OR ==========
    {"OR", {OperandSpec::RM8, OperandSpec::REG8}, EncodingType::MODRM, 0x08},
    {"OR", {OperandSpec::RM16, OperandSpec::REG16}, EncodingType::MODRM, 0x09},
    {"OR", {OperandSpec::REG8, OperandSpec::RM8}, EncodingType::MODRM, 0x0A},
    {"OR", {OperandSpec::REG16, OperandSpec::RM16}, EncodingType::MODRM, 0x0B},
    {"OR", {OperandSpec::AL, OperandSpec::IMM8}, EncodingType::IMMEDIATE, 0x0C},
    {"OR", {OperandSpec::AX, OperandSpec::IMM16}, EncodingType::IMMEDIATE, 0x0D},
    {"OR", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x80, 1},
    {"OR", {OperandSpec::RM16, OperandSpec::IMM16}, EncodingType::MODRM_IMM, 0x81, 1},
    {"OR", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x83, 1},

    // ========== XOR ==========
    {"XOR", {OperandSpec::RM8, OperandSpec::REG8}, EncodingType::MODRM, 0x30},
    {"XOR", {OperandSpec::RM16, OperandSpec::REG16}, EncodingType::MODRM, 0x31},
    {"XOR", {OperandSpec::REG8, OperandSpec::RM8}, EncodingType::MODRM, 0x32},
    {"XOR", {OperandSpec::REG16, OperandSpec::RM16}, EncodingType::MODRM, 0x33},
    {"XOR", {OperandSpec::AL, OperandSpec::IMM8}, EncodingType::IMMEDIATE, 0x34},
    {"XOR", {OperandSpec::AX, OperandSpec::IMM16}, EncodingType::IMMEDIATE, 0x35},
    {"XOR", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x80, 6},
    {"XOR", {OperandSpec::RM16, OperandSpec::IMM16}, EncodingType::MODRM_IMM, 0x81, 6},
    {"XOR", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0x83, 6},

    // ========== NOT ==========
    {"NOT", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xF6, 2},
    {"NOT", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xF7, 2},

    // ========== TEST ==========
    {"TEST", {OperandSpec::RM8, OperandSpec::REG8}, EncodingType::MODRM, 0x84},
    {"TEST", {OperandSpec::RM16, OperandSpec::REG16}, EncodingType::MODRM, 0x85},
    {"TEST", {OperandSpec::AL, OperandSpec::IMM8}, EncodingType::IMMEDIATE, 0xA8},
    {"TEST", {OperandSpec::AX, OperandSpec::IMM16}, EncodingType::IMMEDIATE, 0xA9},
    {"TEST", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xF6, 0},
    {"TEST", {OperandSpec::RM16, OperandSpec::IMM16}, EncodingType::MODRM_IMM, 0xF7, 0},

    // ========== Bit Shifts and Rotates ==========
    // Shift/rotate by 1 (implicit)
    {"ROL", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xD0, 0},
    {"ROL", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xD1, 0},
    {"ROR", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xD0, 1},
    {"ROR", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xD1, 1},
    {"RCL", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xD0, 2},
    {"RCL", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xD1, 2},
    {"RCR", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xD0, 3},
    {"RCR", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xD1, 3},
    {"SHL", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xD0, 4},
    {"SHL", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xD1, 4},
    {"SAL", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xD0, 4},  // Same as SHL
    {"SAL", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xD1, 4},
    {"SHR", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xD0, 5},
    {"SHR", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xD1, 5},
    {"SAR", {OperandSpec::RM8}, EncodingType::MODRM_IMM, 0xD0, 7},
    {"SAR", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xD1, 7},

    // Shift/rotate by 1 (explicit with IMM8 value of 1)
    {"ROL", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD0, 0},
    {"ROL", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD1, 0},
    {"ROR", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD0, 1},
    {"ROR", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD1, 1},
    {"RCL", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD0, 2},
    {"RCL", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD1, 2},
    {"RCR", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD0, 3},
    {"RCR", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD1, 3},
    {"SHL", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD0, 4},
    {"SHL", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD1, 4},
    {"SAL", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD0, 4},
    {"SAL", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD1, 4},
    {"SHR", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD0, 5},
    {"SHR", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD1, 5},
    {"SAR", {OperandSpec::RM8, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD0, 7},
    {"SAR", {OperandSpec::RM16, OperandSpec::IMM8}, EncodingType::MODRM_IMM, 0xD1, 7},

    // Shift/rotate by CL
    {"ROL", {OperandSpec::RM8, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD2, 0},
    {"ROL", {OperandSpec::RM16, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD3, 0},
    {"ROR", {OperandSpec::RM8, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD2, 1},
    {"ROR", {OperandSpec::RM16, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD3, 1},
    {"RCL", {OperandSpec::RM8, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD2, 2},
    {"RCL", {OperandSpec::RM16, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD3, 2},
    {"RCR", {OperandSpec::RM8, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD2, 3},
    {"RCR", {OperandSpec::RM16, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD3, 3},
    {"SHL", {OperandSpec::RM8, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD2, 4},
    {"SHL", {OperandSpec::RM16, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD3, 4},
    {"SAL", {OperandSpec::RM8, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD2, 4},
    {"SAL", {OperandSpec::RM16, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD3, 4},
    {"SHR", {OperandSpec::RM8, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD2, 5},
    {"SHR", {OperandSpec::RM16, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD3, 5},
    {"SAR", {OperandSpec::RM8, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD2, 7},
    {"SAR", {OperandSpec::RM16, OperandSpec::CL}, EncodingType::MODRM_IMM, 0xD3, 7},

    // ========== PUSH ==========
    // Register (0x50-0x57)
    {"PUSH", {OperandSpec::AX}, EncodingType::FIXED, 0x50},
    {"PUSH", {OperandSpec::REG16}, EncodingType::REG_IN_OPCODE, 0x50},
    // Segment registers
    {"PUSH", {OperandSpec::SEGREG}, EncodingType::FIXED, 0x06},  // Will need special handling
    // Memory
    {"PUSH", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xFF, 6},

    // ========== POP ==========
    // Register (0x58-0x5F)
    {"POP", {OperandSpec::AX}, EncodingType::FIXED, 0x58},
    {"POP", {OperandSpec::REG16}, EncodingType::REG_IN_OPCODE, 0x58},
    // Segment registers
    {"POP", {OperandSpec::SEGREG}, EncodingType::FIXED, 0x07},  // Will need special handling
    // Memory
    {"POP", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0x8F, 0},

    // ========== CALL & RET ==========
    {"CALL", {OperandSpec::REL16}, EncodingType::RELATIVE, 0xE8},  // Near call
    {"CALL", {OperandSpec::RM16}, EncodingType::MODRM_IMM, 0xFF, 2},  // Indirect near call
    {"RET", {}, EncodingType::FIXED, 0xC3},      // Near return
    {"RET", {OperandSpec::IMM16}, EncodingType::IMMEDIATE, 0xC2}, // Near return with pop
    {"RETF", {}, EncodingType::FIXED, 0xCB},     // Far return
    {"RETF", {OperandSpec::IMM16}, EncodingType::IMMEDIATE, 0xCA}, // Far return with pop

    // ========== LOOP Instructions ==========
    {"LOOP", {OperandSpec::REL8}, EncodingType::RELATIVE, 0xE2},
    {"LOOPE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0xE1},
    {"LOOPZ", {OperandSpec::REL8}, EncodingType::RELATIVE, 0xE1},
    {"LOOPNE", {OperandSpec::REL8}, EncodingType::RELATIVE, 0xE0},
    {"LOOPNZ", {OperandSpec::REL8}, EncodingType::RELATIVE, 0xE0},
    {"JCXZ", {OperandSpec::REL8}, EncodingType::RELATIVE, 0xE3},

    // ========== INT & IRET ==========
    {"INT", {OperandSpec::IMM8}, EncodingType::IMMEDIATE, 0xCD},
    {"INT3", {}, EncodingType::FIXED, 0xCC},
    {"INTO", {}, EncodingType::FIXED, 0xCE},
    {"IRET", {}, EncodingType::FIXED, 0xCF},

    // ========== String Instructions ==========
    {"MOVSB", {}, EncodingType::FIXED, 0xA4},
    {"MOVSW", {}, EncodingType::FIXED, 0xA5},
    {"CMPSB", {}, EncodingType::FIXED, 0xA6},
    {"CMPSW", {}, EncodingType::FIXED, 0xA7},
    {"SCASB", {}, EncodingType::FIXED, 0xAE},
    {"SCASW", {}, EncodingType::FIXED, 0xAF},
    {"LODSB", {}, EncodingType::FIXED, 0xAC},
    {"LODSW", {}, EncodingType::FIXED, 0xAD},
    {"STOSB", {}, EncodingType::FIXED, 0xAA},
    {"STOSW", {}, EncodingType::FIXED, 0xAB},

    // ========== Repeat Prefixes ==========
    {"REP", {}, EncodingType::FIXED, 0xF3},
    {"REPE", {}, EncodingType::FIXED, 0xF3},
    {"REPZ", {}, EncodingType::FIXED, 0xF3},
    {"REPNE", {}, EncodingType::FIXED, 0xF2},
    {"REPNZ", {}, EncodingType::FIXED, 0xF2},

    // ========== I/O Instructions ==========
    {"IN", {OperandSpec::AL, OperandSpec::IMM8}, EncodingType::IMMEDIATE, 0xE4},
    {"IN", {OperandSpec::AX, OperandSpec::IMM8}, EncodingType::IMMEDIATE, 0xE5},
    {"IN", {OperandSpec::AL, OperandSpec::DX}, EncodingType::FIXED, 0xEC},
    {"IN", {OperandSpec::AX, OperandSpec::DX}, EncodingType::FIXED, 0xED},
    {"OUT", {OperandSpec::IMM8, OperandSpec::AL}, EncodingType::IMMEDIATE, 0xE6},
    {"OUT", {OperandSpec::IMM8, OperandSpec::AX}, EncodingType::IMMEDIATE, 0xE7},
    {"OUT", {OperandSpec::DX, OperandSpec::AL}, EncodingType::FIXED, 0xEE},
    {"OUT", {OperandSpec::DX, OperandSpec::AX}, EncodingType::FIXED, 0xEF},

    // ========== Special/No-operand Instructions ==========
    {"NOP", {}, EncodingType::FIXED, 0x90},
    {"HLT", {}, EncodingType::FIXED, 0xF4},
    {"PUSHA", {}, EncodingType::FIXED, 0x60},
    {"POPA", {}, EncodingType::FIXED, 0x61},
    {"CLC", {}, EncodingType::FIXED, 0xF8},
    {"STC", {}, EncodingType::FIXED, 0xF9},
    {"CMC", {}, EncodingType::FIXED, 0xF5},
    {"CLD", {}, EncodingType::FIXED, 0xFC},
    {"STD", {}, EncodingType::FIXED, 0xFD},
    {"CLI", {}, EncodingType::FIXED, 0xFA},
    {"STI", {}, EncodingType::FIXED, 0xFB},
    {"LAHF", {}, EncodingType::FIXED, 0x9F},
    {"SAHF", {}, EncodingType::FIXED, 0x9E},
    {"PUSHF", {}, EncodingType::FIXED, 0x9C},
    {"POPF", {}, EncodingType::FIXED, 0x9D},
    {"CBW", {}, EncodingType::FIXED, 0x98},
    {"CWD", {}, EncodingType::FIXED, 0x99},
    {"AAA", {}, EncodingType::FIXED, 0x37},
    {"AAS", {}, EncodingType::FIXED, 0x3F},
    {"AAM", {}, EncodingType::FIXED, 0xD4},
    {"AAD", {}, EncodingType::FIXED, 0xD5},
    {"DAA", {}, EncodingType::FIXED, 0x27},
    {"DAS", {}, EncodingType::FIXED, 0x2F},
    {"XLAT", {}, EncodingType::FIXED, 0xD7},
    {"WAIT", {}, EncodingType::FIXED, 0x9B},
    {"LOCK", {}, EncodingType::FIXED, 0xF0},

    // ========== Exchange Instructions ==========
    {"XCHG", {OperandSpec::AX, OperandSpec::REG16}, EncodingType::REG_IN_OPCODE, 0x90},
    {"XCHG", {OperandSpec::REG16, OperandSpec::AX}, EncodingType::REG_IN_OPCODE, 0x90},
    {"XCHG", {OperandSpec::REG8, OperandSpec::RM8}, EncodingType::MODRM, 0x86},
    {"XCHG", {OperandSpec::REG16, OperandSpec::RM16}, EncodingType::MODRM, 0x87},

    // ========== Load Effective Address ==========
    {"LEA", {OperandSpec::REG16, OperandSpec::MEM16}, EncodingType::MODRM, 0x8D},
    {"LDS", {OperandSpec::REG16, OperandSpec::MEM16}, EncodingType::MODRM, 0xC5},
    {"LES", {OperandSpec::REG16, OperandSpec::MEM16}, EncodingType::MODRM, 0xC4},
};

} // namespace e2asm
