#include "instruction_encoder.h"
#include "modrm_generator.h"
#include "../parser/expression_parser.h"
#include <algorithm>
#include <cctype>

namespace e2asm {

// Helper: Case-insensitive string compare
static bool iequals(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
        [](char a, char b) { return std::tolower(a) == std::tolower(b); });
}

InstructionEncoder::InstructionEncoder() {
}

EncodedInstruction InstructionEncoder::encode(const Instruction* instr) {
    const InstructionEncoding* encoding = findEncoding(instr->mnemonic, instr->operands);

    if (!encoding) {
        return EncodedInstruction("No encoding found for instruction: " + instr->mnemonic);
    }

    switch (encoding->encoding_type) {
        case EncodingType::MODRM:
            return encodeModRM(encoding, instr);

        case EncodingType::REG_IN_OPCODE:
            return encodeRegInOpcode(encoding, instr);

        case EncodingType::IMMEDIATE:
            return encodeImmediate(encoding, instr);

        case EncodingType::MODRM_IMM:
            return encodeModRMImm(encoding, instr);

        case EncodingType::RELATIVE:
            return encodeRelative(encoding, instr);

        case EncodingType::FIXED:
            // Just return the opcode
            return EncodedInstruction(std::vector<uint8_t>{encoding->base_opcode});

        default:
            return EncodedInstruction(std::string("Unsupported encoding type"));
    }
}

const InstructionEncoding* InstructionEncoder::findEncoding(
    const std::string& mnemonic,
    const std::vector<std::unique_ptr<Operand>>& operands
) {
    const InstructionEncoding* best_match = nullptr;
    int best_specificity = -1;

    // Search instruction table for matching encoding
    // Prefer more specific matches (AL/AX) over generic ones (REG8/REG16)
    for (const auto& encoding : INSTRUCTION_TABLE) {
        // Check mnemonic (case-insensitive)
        if (!iequals(encoding.mnemonic, mnemonic)) {
            continue;
        }

        // Check operand count
        if (encoding.operands.size() != operands.size()) {
            continue;
        }

        // Check each operand matches the spec
        bool all_match = true;
        int specificity = 0;

        for (size_t i = 0; i < operands.size(); i++) {
            if (!matchesSpec(operands[i].get(), encoding.operands[i])) {
                all_match = false;
                break;
            }

            // Calculate specificity score (higher = more specific)
            // Specific registers (AL/AX) = 10
            // Generic registers (REG8/REG16) = 5
            // RM = 3
            // Other = 1
            switch (encoding.operands[i]) {
                case OperandSpec::AL:
                case OperandSpec::AX:
                case OperandSpec::CL:
                case OperandSpec::DX:
                    specificity += 10;
                    break;
                case OperandSpec::REG8:
                case OperandSpec::REG16:
                case OperandSpec::SEGREG:
                    specificity += 5;
                    break;
                case OperandSpec::RM8:
                case OperandSpec::RM16:
                    specificity += 3;
                    break;
                default:
                    specificity += 1;
                    break;
            }
        }

        if (all_match && specificity > best_specificity) {
            best_match = &encoding;
            best_specificity = specificity;
        }
    }

    return best_match;
}

bool InstructionEncoder::matchesSpec(const Operand* operand, OperandSpec spec) {
    auto* reg = dynamic_cast<const RegisterOperand*>(operand);
    auto* imm = dynamic_cast<const ImmediateOperand*>(operand);
    auto* mem = dynamic_cast<const MemoryOperand*>(operand);
    auto* label = dynamic_cast<const LabelRef*>(operand);

    switch (spec) {
        case OperandSpec::REG8:
            return reg && reg->size == 8 && !reg->is_segment;

        case OperandSpec::REG16:
            return reg && reg->size == 16 && !reg->is_segment;

        case OperandSpec::MEM8:
            // Pure memory operand (direct address only - for special accumulator encoding)
            // Non-direct memory should use RM8
            return mem && mem->is_direct_address;

        case OperandSpec::MEM16:
            // Pure memory operand (direct address only - for special accumulator encoding)
            // Non-direct memory (with registers or labels) should use RM16
            // Exception: Memory with just a label (no registers) counts as direct address
            // Also allow plain label references (for LEA instruction)
            if (label) return true;  // Plain label reference (e.g., "lea si, data")
            if (!mem) return false;
            if (mem->is_direct_address) return true;
            if (mem->parsed_address && mem->parsed_address->registers.empty()) return true;
            return false;

        case OperandSpec::RM8:
            // Register or memory (8-bit)
            if (mem) {
                // If size_hint is specified, it must match
                if (mem->size_hint != 0 && mem->size_hint != 8) return false;
                return true;
            }
            if (reg && reg->size == 8 && !reg->is_segment) return true;
            return false;

        case OperandSpec::RM16:
            // Register or memory (16-bit)
            if (mem) {
                // If size_hint is specified, it must match
                if (mem->size_hint != 0 && mem->size_hint != 16) return false;
                return true;
            }
            if (reg && reg->size == 16 && !reg->is_segment) return true;
            return false;

        case OperandSpec::IMM8:
            // Respect the size_hint
            // If size_hint is 16, don't match IMM8
            if (imm && imm->size_hint == 16) return false;
            return (imm && (imm->value >= -128 && imm->value <= 255)) || label;

        case OperandSpec::IMM16:
            // Respect the size_hint
            // If size_hint is 8, don't match IMM16 (user explicitly wants byte)
            if (imm && imm->size_hint == 8) return false;
            return (imm && (imm->value >= -32768 && imm->value <= 65535)) || label;

        case OperandSpec::AL:
            return reg && reg->size == 8 && reg->code == 0;

        case OperandSpec::AX:
            return reg && reg->size == 16 && reg->code == 0 && !reg->is_segment;

        case OperandSpec::SEGREG:
            return reg && reg->is_segment;

        case OperandSpec::CL:
            return reg && reg->size == 8 && reg->code == 1;

        case OperandSpec::DX:
            return reg && reg->size == 16 && reg->code == 2 && !reg->is_segment;

        case OperandSpec::REL8:
            // Label references for SHORT jumps
            if (label) {
                return label->jump_type == LabelRef::JumpType::SHORT;
            }
            return false;

        case OperandSpec::REL16:
            // Label references for NEAR jumps (or unspecified, which defaults to NEAR)
            if (label) {
                return label->jump_type == LabelRef::JumpType::NEAR ||
                       label->jump_type == LabelRef::JumpType::FAR;  // FAR also uses 16-bit for now
            }
            return false;

        case OperandSpec::LABEL:
            // Generic label reference (any type)
            return label != nullptr;

        case OperandSpec::NONE:
            return false;  // Should not be matched

        default:
            return false;
    }
}

OperandSpec InstructionEncoder::classifyOperand(const Operand* operand) {
    auto* reg = dynamic_cast<const RegisterOperand*>(operand);
    auto* imm = dynamic_cast<const ImmediateOperand*>(operand);
    auto* mem = dynamic_cast<const MemoryOperand*>(operand);
    auto* label = dynamic_cast<const LabelRef*>(operand);

    if (reg) {
        if (reg->is_segment) return OperandSpec::SEGREG;
        if (reg->size == 8) {
            if (reg->code == 0) return OperandSpec::AL;
            return OperandSpec::REG8;
        }
        if (reg->size == 16) {
            if (reg->code == 0) return OperandSpec::AX;
            return OperandSpec::REG16;
        }
    }

    if (imm) {
        if (imm->value >= -128 && imm->value <= 255) return OperandSpec::IMM8;
        return OperandSpec::IMM16;
    }

    if (label) {
        // Label references are treated as 16-bit immediates
        return OperandSpec::IMM16;
    }

    if (mem) {
        // For now, assume 16-bit memory (will be refined later)
        return OperandSpec::MEM16;
    }

    return OperandSpec::NONE;
}

EncodedInstruction InstructionEncoder::encodeModRM(
    const InstructionEncoding* encoding,
    const Instruction* instr
) {
    std::vector<uint8_t> bytes;

    auto* dest_reg = dynamic_cast<const RegisterOperand*>(instr->operands[0].get());
    auto* src_reg = dynamic_cast<const RegisterOperand*>(instr->operands[1].get());
    auto* dest_mem = dynamic_cast<const MemoryOperand*>(instr->operands[0].get());
    auto* src_mem = dynamic_cast<const MemoryOperand*>(instr->operands[1].get());
    auto* src_label = dynamic_cast<const LabelRef*>(instr->operands[1].get());

    // Add segment override prefix if present in any memory operand
    const MemoryOperand* mem_op = dest_mem ? dest_mem : src_mem;
    if (mem_op && mem_op->segment_override) {
        auto prefix = getSegmentOverridePrefix(*mem_op->segment_override);
        if (prefix) {
            bytes.push_back(*prefix);
        }
    }

    bytes.push_back(encoding->base_opcode);

    if (dest_reg && src_reg) {
        // Register-to-register: MOD = 0x03 (11b)
        uint8_t modrm = ModRMGenerator::generateRegToReg(src_reg->code, dest_reg->code);
        bytes.push_back(modrm);
    }
    else if (dest_reg && src_label) {
        // Register with label (e.g., LEA SI, data) - treat as direct memory address
        auto symbol = lookupLabel(src_label->label);
        if (!symbol || !symbol->is_resolved) {
            return EncodedInstruction("Undefined label: " + src_label->label);
        }

        ModRMResult result = ModRMGenerator::generateDirect(symbol->value, dest_reg->code);
        if (!result.success) {
            return EncodedInstruction(result.error);
        }

        bytes.push_back(result.modrm_byte);
        bytes.insert(bytes.end(), result.displacement.begin(), result.displacement.end());
    }
    else if (dest_mem && src_reg) {
        // Register to memory: [mem], reg
        ModRMResult result;
        if (dest_mem->is_direct_address) {
            result = ModRMGenerator::generateDirect(dest_mem->direct_address_value, src_reg->code);
        } else if (dest_mem->parsed_address) {
            result = ModRMGenerator::generateMemory(*dest_mem->parsed_address, src_reg->code, m_symbol_table);
        } else {
            return EncodedInstruction(std::string("Invalid memory operand"));
        }

        if (!result.success) {
            return EncodedInstruction(result.error);
        }

        bytes.push_back(result.modrm_byte);
        bytes.insert(bytes.end(), result.displacement.begin(), result.displacement.end());
    }
    else if (dest_reg && src_mem) {
        // Memory to register: reg, [mem]
        ModRMResult result;
        if (src_mem->is_direct_address) {
            result = ModRMGenerator::generateDirect(src_mem->direct_address_value, dest_reg->code);
        } else if (src_mem->parsed_address) {
            result = ModRMGenerator::generateMemory(*src_mem->parsed_address, dest_reg->code, m_symbol_table);
        } else {
            return EncodedInstruction(std::string("Invalid memory operand"));
        }

        if (!result.success) {
            return EncodedInstruction(result.error);
        }

        bytes.push_back(result.modrm_byte);
        bytes.insert(bytes.end(), result.displacement.begin(), result.displacement.end());
    }
    else {
        return EncodedInstruction(std::string("Invalid operand combination for ModRM"));
    }

    return EncodedInstruction(bytes);
}

EncodedInstruction InstructionEncoder::encodeRegInOpcode(
    const InstructionEncoding* encoding,
    const Instruction* instr
) {
    std::vector<uint8_t> bytes;

    // Get register code from first operand
    auto* reg = dynamic_cast<const RegisterOperand*>(instr->operands[0].get());
    if (!reg) {
        return EncodedInstruction("Expected register operand");
    }

    // Opcode = base_opcode + register_code
    bytes.push_back(encoding->base_opcode + reg->code);

    // Check for immediate value (optional - some instructions like PUSH/POP/INC/DEC don't have it)
    if (instr->operands.size() > 1) {
        auto* imm = dynamic_cast<const ImmediateOperand*>(instr->operands[1].get());
        auto* label = dynamic_cast<const LabelRef*>(instr->operands[1].get());
        auto* reg2 = dynamic_cast<const RegisterOperand*>(instr->operands[1].get());

        // Special case: XCHG AX, reg16 - second operand is a register, encoded in opcode
        if (reg2) {
            // For XCHG AX, reg - use second register's code instead
            bytes[0] = encoding->base_opcode + reg2->code;
            return EncodedInstruction(bytes);
        }

        int64_t value = 0;
        if (imm) {
            // Check if immediate has a label reference or expression
            if (imm->has_label) {
                // Check if it contains operators (is an expression)
                bool is_expression = (imm->label_name.find('+') != std::string::npos ||
                                     imm->label_name.find('-') != std::string::npos ||
                                     imm->label_name.find('*') != std::string::npos ||
                                     imm->label_name.find('/') != std::string::npos);

                if (is_expression) {
                    // Evaluate expression
                    auto eval_result = evaluateExpression(imm->label_name);
                    if (!eval_result) {
                        return EncodedInstruction("Invalid expression: " + imm->label_name);
                    }
                    value = *eval_result;
                } else {
                    // Simple label lookup
                    auto symbol = lookupLabel(imm->label_name);
                    if (!symbol || !symbol->is_resolved) {
                        return EncodedInstruction("Undefined label: " + imm->label_name);
                    }
                    value = symbol->value;
                }
            } else {
                value = imm->value;
            }
        } else if (label) {
            // Resolve label to address
            auto symbol = lookupLabel(label->label);
            if (!symbol || !symbol->is_resolved) {
                return EncodedInstruction("Undefined label: " + label->label);
            }
            value = symbol->value;
        } else {
            return EncodedInstruction("Expected immediate operand or label reference");
        }

        // Encode immediate (8-bit or 16-bit based on register size)
        size_t imm_size = (reg->size == 8) ? 1 : 2;
        auto imm_bytes = encodeImmediate(value, imm_size);
        bytes.insert(bytes.end(), imm_bytes.begin(), imm_bytes.end());
    }

    return EncodedInstruction(bytes);
}

EncodedInstruction InstructionEncoder::encodeImmediate(
    const InstructionEncoding* encoding,
    const Instruction* instr
) {
    std::vector<uint8_t> bytes;

    bytes.push_back(encoding->base_opcode);

    // Handle different operand combinations
    if (instr->operands.size() == 0) {
        // No operands - just opcode (shouldn't happen for IMMEDIATE encoding, but handle it)
        return EncodedInstruction(bytes);
    }

    // Single operand (e.g., INT 3, RET imm16)
    if (instr->operands.size() == 1) {
        auto* imm = dynamic_cast<const ImmediateOperand*>(instr->operands[0].get());
        if (imm) {
            int64_t value = imm->value;
            // Check if immediate has a label reference or expression
            if (imm->has_label) {
                // Check if it contains operators (is an expression)
                bool is_expression = (imm->label_name.find('+') != std::string::npos ||
                                     imm->label_name.find('-') != std::string::npos ||
                                     imm->label_name.find('*') != std::string::npos ||
                                     imm->label_name.find('/') != std::string::npos);

                if (is_expression) {
                    auto eval_result = evaluateExpression(imm->label_name);
                    if (!eval_result) {
                        return EncodedInstruction("Invalid expression: " + imm->label_name);
                    }
                    value = *eval_result;
                } else {
                    // Simple label lookup
                    auto symbol = lookupLabel(imm->label_name);
                    if (!symbol || !symbol->is_resolved) {
                        return EncodedInstruction("Undefined label: " + imm->label_name);
                    }
                    value = symbol->value;
                }
            }
            size_t imm_size = (encoding->operands[0] == OperandSpec::IMM8) ? 1 : 2;
            auto imm_bytes = encodeImmediate(value, imm_size);
            bytes.insert(bytes.end(), imm_bytes.begin(), imm_bytes.end());
            return EncodedInstruction(bytes);
        }
    }

    // Two operands - check for immediate value OR direct memory address (for MOV AL/AX, [addr])
    if (instr->operands.size() >= 2) {
        auto* imm = dynamic_cast<const ImmediateOperand*>(instr->operands[1].get());
        auto* mem = dynamic_cast<const MemoryOperand*>(instr->operands[1].get());

        // Also check first operand for OUT imm8, AL/AX
        auto* imm0 = dynamic_cast<const ImmediateOperand*>(instr->operands[0].get());
        auto* mem0 = dynamic_cast<const MemoryOperand*>(instr->operands[0].get());

        if (imm0) {
            // First operand is immediate (e.g., OUT imm8, AL)
            int64_t value = imm0->value;
            if (imm0->has_label) {
                auto symbol = lookupLabel(imm0->label_name);
                if (!symbol || !symbol->is_resolved) {
                    return EncodedInstruction("Undefined label: " + imm0->label_name);
                }
                value = symbol->value;
            }
            size_t imm_size = (encoding->operands[0] == OperandSpec::IMM8) ? 1 : 2;
            auto imm_bytes = encodeImmediate(value, imm_size);
            bytes.insert(bytes.end(), imm_bytes.begin(), imm_bytes.end());
            return EncodedInstruction(bytes);
        }
        else if (mem0) {
          // First operand is memory (e.g., MOV [MEM16], AX)
          if (mem0->is_direct_address) {
            auto addr_bytes = encodeImmediate(mem0->direct_address_value, 2);
            bytes.insert(bytes.end(), addr_bytes.begin(), addr_bytes.end());
            return EncodedInstruction(bytes);
          }
          else if (mem0->parsed_address && mem0->parsed_address->registers.empty()) {
            int64_t address = mem0->parsed_address->displacement;
            if (mem0->parsed_address->has_label) {
              auto symbol = lookupLabel(mem0->parsed_address->label_name);
              if (!symbol || !symbol->is_resolved) {
                return EncodedInstruction("Undefined label: " + mem0->parsed_address->label_name);
              }
              address += symbol->value;
            }
            auto addr_bytes = encodeImmediate(address, 2);
            bytes.insert(bytes.end(), addr_bytes.begin(), addr_bytes.end());
            return EncodedInstruction(bytes);
          }
        }
        else if (imm) {
            // Second operand is immediate
            int64_t value = imm->value;
            if (imm->has_label) {
                auto symbol = lookupLabel(imm->label_name);
                if (!symbol || !symbol->is_resolved) {
                    return EncodedInstruction("Undefined label: " + imm->label_name);
                }
                value = symbol->value;
            }
            size_t imm_size = (encoding->operands[1] == OperandSpec::IMM8) ? 1 : 2;
            auto imm_bytes = encodeImmediate(value, imm_size);
            bytes.insert(bytes.end(), imm_bytes.begin(), imm_bytes.end());
            return EncodedInstruction(bytes);
        }
        else if (mem) {
            // Memory operand - check if it's a valid direct address
            if (mem->is_direct_address) {
                // Numeric direct address (MOV AL/AX, [0x1234])
                auto addr_bytes = encodeImmediate(mem->direct_address_value, 2);
                bytes.insert(bytes.end(), addr_bytes.begin(), addr_bytes.end());
                return EncodedInstruction(bytes);
            }
            else if (mem->parsed_address) {
                // Check if it's a direct address (no registers, just displacement/label)
                const auto& addr = *mem->parsed_address;
                if (addr.registers.empty()) {
                    // Direct address - resolve label if present
                    int64_t address = addr.displacement;
                    if (addr.has_label) {
                        auto symbol = lookupLabel(addr.label_name);
                        if (!symbol || !symbol->is_resolved) {
                            return EncodedInstruction("Undefined label: " + addr.label_name);
                        }
                        address += symbol->value;
                    }
                    auto addr_bytes = encodeImmediate(address, 2);
                    bytes.insert(bytes.end(), addr_bytes.begin(), addr_bytes.end());
                    return EncodedInstruction(bytes);
                }
            }
        }
    }

    return EncodedInstruction("Expected immediate operand or direct address");
}

EncodedInstruction InstructionEncoder::encodeModRMImm(
    const InstructionEncoding* encoding,
    const Instruction* instr
) {
    std::vector<uint8_t> bytes;

    // Generate ModR/M byte
    auto* dest_reg = dynamic_cast<const RegisterOperand*>(instr->operands[0].get());
    auto* dest_mem = dynamic_cast<const MemoryOperand*>(instr->operands[0].get());

    // Add segment override prefix if present in memory operand
    if (dest_mem && dest_mem->segment_override) {
        auto prefix = getSegmentOverridePrefix(*dest_mem->segment_override);
        if (prefix) {
            bytes.push_back(*prefix);
        }
    }

    // Add opcode
    bytes.push_back(encoding->base_opcode);

    if (dest_reg) {
        // Destination is register
        uint8_t modrm = ModRMGenerator::generateRegToReg(encoding->modrm_reg_field, dest_reg->code);
        bytes.push_back(modrm);
    }
    else if (dest_mem) {
        // Destination is memory
        ModRMResult result;
        if (dest_mem->is_direct_address) {
            result = ModRMGenerator::generateDirect(dest_mem->direct_address_value, encoding->modrm_reg_field);
        } else if (dest_mem->parsed_address) {
            result = ModRMGenerator::generateMemory(*dest_mem->parsed_address, encoding->modrm_reg_field, m_symbol_table);
        } else {
            return EncodedInstruction(std::string("Invalid memory operand"));
        }

        if (!result.success) {
            return EncodedInstruction(result.error);
        }

        bytes.push_back(result.modrm_byte);
        bytes.insert(bytes.end(), result.displacement.begin(), result.displacement.end());
    }
    else {
        return EncodedInstruction(std::string("Invalid destination operand"));
    }

    // Add immediate value (if there is one - some instructions like INC/DEC/NEG/NOT/MUL/DIV don't have it)
    if (instr->operands.size() > 1) {
        auto* imm = dynamic_cast<const ImmediateOperand*>(instr->operands[1].get());

        // Check for CL operand (for shifts/rotates)
        if (!imm) {
            auto* cl_reg = dynamic_cast<const RegisterOperand*>(instr->operands[1].get());
            if (cl_reg && cl_reg->code == 1 && cl_reg->size == 8) {
                // Shift by CL - no immediate to encode
                return EncodedInstruction(bytes);
            }
            return EncodedInstruction(std::string("Expected immediate operand"));
        }

        // Resolve immediate value (could be label reference or expression)
        int64_t value = imm->value;
        if (imm->has_label) {
            // Check if it contains operators (is an expression)
            bool is_expression = (imm->label_name.find('+') != std::string::npos ||
                                 imm->label_name.find('-') != std::string::npos ||
                                 imm->label_name.find('*') != std::string::npos ||
                                 imm->label_name.find('/') != std::string::npos);

            if (is_expression) {
                // Evaluate expression
                auto eval_result = evaluateExpression(imm->label_name);
                if (!eval_result) {
                    return EncodedInstruction("Invalid expression: " + imm->label_name);
                }
                value = *eval_result;
            } else {
                // Simple label lookup
                auto symbol = lookupLabel(imm->label_name);
                if (!symbol || !symbol->is_resolved) {
                    return EncodedInstruction("Undefined label: " + imm->label_name);
                }
                value = symbol->value;
            }
        }

        // Special case: shift/rotate by 1 using D0/D1 opcodes - immediate is implicit, don't encode it
        if ((encoding->base_opcode == 0xD0 || encoding->base_opcode == 0xD1) && value == 1) {
            // Shift by 1 - no immediate to encode (it's implicit in the opcode)
            return EncodedInstruction(bytes);
        }

        // Determine size from operand spec
        size_t imm_size = (encoding->operands[1] == OperandSpec::IMM8) ? 1 : 2;
        auto imm_bytes = encodeImmediate(value, imm_size);
        bytes.insert(bytes.end(), imm_bytes.begin(), imm_bytes.end());
    }

    return EncodedInstruction(bytes);
}

EncodedInstruction InstructionEncoder::encodeRelative(
    const InstructionEncoding* encoding,
    const Instruction* instr
) {
    std::vector<uint8_t> bytes;

    // Get label reference
    auto* label_ref = dynamic_cast<const LabelRef*>(instr->operands[0].get());
    if (!label_ref) {
        return EncodedInstruction("Expected label operand for jump");
    }

    // Look up label address in symbol table
    auto symbol = lookupLabel(label_ref->label);
    if (!symbol) {
        return EncodedInstruction("Undefined label: " + label_ref->label);
    }

    // Determine displacement size from encoding
    size_t disp_size = (encoding->operands[0] == OperandSpec::REL8) ? 1 : 2;

    // Calculate initial displacement
    uint64_t instruction_size = 1 + disp_size;
    uint64_t next_instruction_address = m_current_address + instruction_size;
    int64_t displacement = static_cast<int64_t>(symbol->value) - static_cast<int64_t>(next_instruction_address);

    // Auto-upgrade SHORT to NEAR for JMP if displacement doesn't fit
    // (Conditional jumps cannot be upgraded - they only support SHORT on 8086)
    std::string mnemonic_upper = instr->mnemonic;
    std::transform(mnemonic_upper.begin(), mnemonic_upper.end(), mnemonic_upper.begin(), ::toupper);

    if (disp_size == 1 && (displacement < -128 || displacement > 127)) {
        // Check if this is JMP (can be upgraded to NEAR)
        if (mnemonic_upper == "JMP") {
            // Auto-upgrade to NEAR (3 bytes: opcode + 16-bit displacement)
            disp_size = 2;
            bytes.push_back(0xE9);  // NEAR JMP opcode

            // Recalculate displacement with new instruction size
            instruction_size = 1 + disp_size;
            next_instruction_address = m_current_address + instruction_size;
            displacement = static_cast<int64_t>(symbol->value) - static_cast<int64_t>(next_instruction_address);
        } else {
            // Conditional jump - cannot upgrade, must error
            return EncodedInstruction("Jump target too far for SHORT jump (distance: " +
                std::to_string(displacement) + ", max: ±127)");
        }
    } else {
        // Use encoding's opcode
        bytes.push_back(encoding->base_opcode);
    }

    // Encode displacement
    auto disp_bytes = encodeImmediate(displacement, disp_size);
    bytes.insert(bytes.end(), disp_bytes.begin(), disp_bytes.end());

    return EncodedInstruction(bytes);
}

uint8_t InstructionEncoder::generateModRM(uint8_t mod, uint8_t reg, uint8_t rm) {
    return ((mod & 0x03) << 6) | ((reg & 0x07) << 3) | (rm & 0x07);
}

std::vector<uint8_t> InstructionEncoder::encodeImmediate(int64_t value, size_t size_bytes) {
    std::vector<uint8_t> bytes;

    // Little-endian encoding
    for (size_t i = 0; i < size_bytes; i++) {
        bytes.push_back(static_cast<uint8_t>(value & 0xFF));
        value >>= 8;
    }

    return bytes;
}

bool InstructionEncoder::isAccumulator(const Operand* operand) {
    auto* reg = dynamic_cast<const RegisterOperand*>(operand);
    return reg && reg->code == 0;  // AL (code 0) or AX (code 0)
}

std::optional<Symbol> InstructionEncoder::lookupLabel(const std::string& label_name) const {
    if (!m_symbol_table) {
        return std::nullopt;
    }

    // Try normal lookup first (with scope applied)
    auto symbol = m_symbol_table->lookup(label_name);

    // If not found and label starts with '.', try direct lookup (without scope)
    // This handles segment names like .text and .data as global labels
    if (!symbol && !label_name.empty() && label_name[0] == '.') {
        symbol = m_symbol_table->lookupDirect(label_name);
    }

    return symbol;
}

std::optional<uint8_t> InstructionEncoder::getSegmentOverridePrefix(const std::string& segment) const {
    // Convert to uppercase for comparison
    std::string seg_upper = segment;
    for (char& c : seg_upper) c = std::toupper(c);

    // Return appropriate segment override prefix byte
    if (seg_upper == "ES") return 0x26;
    if (seg_upper == "CS") return 0x2E;
    if (seg_upper == "SS") return 0x36;
    if (seg_upper == "DS") return 0x3E;

    return std::nullopt;  // Invalid segment
}

std::optional<int64_t> InstructionEncoder::evaluateExpression(const std::string& expr) const {
    // Substitute EQU constants in the expression
    std::string substituted = expr;

    // Get all symbols from the symbol table
    if (m_symbol_table) {
        auto symbols = m_symbol_table->getAllSymbols();

        // Replace each symbol with its value
        for (const auto& [name, symbol] : symbols) {
            // Only substitute constants (EQU directives)
            if (symbol.type == SymbolType::CONSTANT && symbol.is_resolved) {
                size_t pos = 0;
                while ((pos = substituted.find(name, pos)) != std::string::npos) {
                    // Check if it's a whole word (not part of another identifier)
                    bool is_word_start = (pos == 0 || (!std::isalnum(substituted[pos - 1]) && substituted[pos - 1] != '_'));
                    bool is_word_end = (pos + name.size() >= substituted.size() ||
                                       (!std::isalnum(substituted[pos + name.size()]) && substituted[pos + name.size()] != '_'));

                    if (is_word_start && is_word_end) {
                        substituted.replace(pos, name.size(), std::to_string(symbol.value));
                        pos += std::to_string(symbol.value).size();
                    } else {
                        ++pos;
                    }
                }
            }
        }
    }

    // Evaluate the substituted expression using ExpressionParser
    return ExpressionParser::evaluate(substituted);
}

} // namespace e2asm
