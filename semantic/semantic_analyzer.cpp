#include "semantic_analyzer.h"
#include "../parser/expression_parser.h"
#include <algorithm>
#include <functional>

namespace e2asm {

SemanticAnalyzer::SemanticAnalyzer()
    : m_current_address(0)
    , m_segment_start_address(0)
    , m_origin_address(0)
    , m_last_was_terminator(false)
{
}

void SemanticAnalyzer::clear() {
    m_symbol_table.clear();
    m_addresses.clear();
    m_errors.clear();
    m_current_address = 0;
    m_segments.clear();
    m_current_segment.clear();
    m_segment_start_address = 0;
    m_origin_address = 0;
    m_last_was_terminator = false;
}

bool SemanticAnalyzer::analyze(Program* program) {
    clear();

    if (!pass1_buildSymbols(program)) {
        return false;
    }

    /**
      Pass 2 is doing a single iteration for now
      TODO: optimize this
    */
    pass2_resolveSymbols(program);

    for (const auto& [name, symbol] : m_symbol_table.getAllSymbols()) {
        if (!symbol.is_resolved) {
            error("Undefined symbol: " + name, SourceLocation());
        }
    }

    return m_errors.empty();
}

bool SemanticAnalyzer::pass1_buildSymbols(Program* program) {
    m_current_address = m_origin_address;
    m_segment_start_address = m_origin_address;
    m_addresses.clear();

    for (size_t i = 0; i < program->statements.size(); i++) {
        auto& stmt = program->statements[i];

        // Handle labels
        if (auto* label = dynamic_cast<Label*>(stmt.get())) {
            // If this is a global label (doesn't start with '.'), update the scope
            if (!SymbolTable::isLocalLabel(label->name)) {
                m_symbol_table.setGlobalScope(label->name);
            }

            // Define label at current address (will be scoped if local)
            if (!m_symbol_table.define(label->name, SymbolType::LABEL, m_current_address, label->location.line)) {
                error("Label '" + label->name + "' already defined", label->location);
                return false;
            }

            // Labels don't consume space, but record them
            m_addresses.push_back({i, m_current_address, 0});
            continue;
        }

        // Handle EQU directives
        if (auto* equ = dynamic_cast<EQUDirective*>(stmt.get())) {
            // Define constant
            if (!m_symbol_table.define(equ->name, SymbolType::CONSTANT, equ->value, equ->location.line)) {
                error("Constant '" + equ->name + "' already defined", equ->location);
                return false;
            }

            // EQU doesn't consume space
            m_addresses.push_back({i, m_current_address, 0});
            continue;
        }

        // Handle ORG directive
        if (auto* org = dynamic_cast<ORGDirective*>(stmt.get())) {
            setOrigin(org->address);
            m_addresses.push_back({i, m_current_address, 0});
            continue;
        }

        // Handle SEGMENT directive
        if (auto* seg = dynamic_cast<SEGMENTDirective*>(stmt.get())) {
            enterSegment(seg->name);

            // Define the segment name as a label pointing to segment start
            // Temporarily clear global scope so segment names like .data aren't treated as local labels
            std::string saved_scope = m_symbol_table.getGlobalScope();
            m_symbol_table.setGlobalScope("");  // Clear scope for segment label

            if (!m_symbol_table.define(seg->name, SymbolType::LABEL, m_current_address, seg->location.line)) {
                // If already defined, update it instead
                // Does it work this way in NASM? idk
                m_symbol_table.update(seg->name, m_current_address);
            }

            m_symbol_table.setGlobalScope(saved_scope);  // Restore scope

            m_addresses.push_back({i, m_current_address, 0});
            continue;
        }

        // Handle ENDS directive
        if (auto* ends = dynamic_cast<ENDSDirective*>(stmt.get())) {
            exitSegment(ends->name);
            m_addresses.push_back({i, m_current_address, 0});
            continue;
        }

        // Handle RES* directives
        if (auto* res = dynamic_cast<RESDirective*>(stmt.get())) {
            size_t element_size = 0;
            switch (res->size) {
                case RESDirective::Size::BYTE: element_size = 1; break;
                case RESDirective::Size::WORD: element_size = 2; break;
                case RESDirective::Size::DWORD: element_size = 4; break;
                case RESDirective::Size::QWORD: element_size = 8; break;
                case RESDirective::Size::TBYTE: element_size = 10; break;
            }

            uint64_t total_size = element_size * res->count;
            m_addresses.push_back({i, m_current_address, total_size});
            m_current_address += total_size;
            continue;
        }

        // Handle TIMES directive
        if (auto* times = dynamic_cast<TIMESDirective*>(stmt.get())) {
            // Resolve symbolic count if needed (count == -1 means unresolved)
            if (times->count < 0) {
                int64_t resolved_count;
                if (!resolveSymbol(times->count_expr, times->location, resolved_count)) {
                    return false;
                }
                times->count = resolved_count;
            }

            // Calculate size of the repeated node
            uint64_t single_size = 0;

            if (auto* data = dynamic_cast<DataDirective*>(times->repeated_node.get())) {
                // Resolve any symbols in the data directive first
                if (!resolveDataSymbols(data)) {
                    return false;
                }

                size_t element_size = 0;
                switch (data->size) {
                    case DataDirective::Size::BYTE: element_size = 1; break;
                    case DataDirective::Size::WORD: element_size = 2; break;
                    case DataDirective::Size::DWORD: element_size = 4; break;
                    case DataDirective::Size::QWORD: element_size = 8; break;
                    case DataDirective::Size::TBYTE: element_size = 10; break;
                }
                for (const auto& value : data->values) {
                    if (value.type == DataValue::Type::STRING) {
                        single_size += value.string_value.length();
                    } else if (value.type == DataValue::Type::CHARACTER) {
                        single_size += 1;
                    } else {
                        single_size += element_size;
                    }
                }
            } else if (auto* instr = dynamic_cast<Instruction*>(times->repeated_node.get())) {
                single_size = calculateInstructionSize(instr);
            }

            uint64_t total_size = single_size * times->count;
            m_addresses.push_back({i, m_current_address, total_size});
            m_current_address += total_size;
            continue;
        }

        // Handle data directives
        if (auto* data = dynamic_cast<DataDirective*>(stmt.get())) {
            // Resolve any symbols in the data directive
            if (!resolveDataSymbols(data)) {
                return false;
            }

            uint64_t size = 0;

            // Calculate size based on directive and values
            size_t element_size = 0;
            switch (data->size) {
                case DataDirective::Size::BYTE: element_size = 1; break;
                case DataDirective::Size::WORD: element_size = 2; break;
                case DataDirective::Size::DWORD: element_size = 4; break;
                case DataDirective::Size::QWORD: element_size = 8; break;
                case DataDirective::Size::TBYTE: element_size = 10; break;
            }

            for (const auto& value : data->values) {
                if (value.type == DataValue::Type::STRING) {
                    size += value.string_value.length();
                } else if (value.type == DataValue::Type::CHARACTER) {
                    size += 1;
                } else {
                    size += element_size;
                }
            }

            m_addresses.push_back({i, m_current_address, size});
            m_current_address += size;
            continue;
        }

        // Handle instructions
        if (auto* instr = dynamic_cast<Instruction*>(stmt.get())) {
            // Resolve memory operand expressions (EQU constants, etc.)
            if (!resolveMemoryOperands(instr)) {
                return false;
            }

            // Record address for this instruction
            uint64_t size = calculateInstructionSize(instr);
            instr->assigned_address = m_current_address;  // Store address in instruction
            instr->estimated_size = size;                // Store estimated size
            m_addresses.push_back({i, m_current_address, size});
            m_current_address += size;

            // Check if this instruction terminates control flow
            // TODO: This is garbage, but it works so I will refine it later
            std::string mnem = instr->mnemonic;
            std::transform(mnem.begin(), mnem.end(), mnem.begin(), ::toupper);
            if ((mnem == "HLT" || mnem == "RET" || mnem == "RETF" ||
                mnem == "IRET" || mnem == "JMP" ||
                mnem == "INT") && instr->operands.size() >= 1) {
                m_last_was_terminator = true;
            } else {
                m_last_was_terminator = false;
            }

            continue;
        }
    }

    return true;
}

bool SemanticAnalyzer::pass2_resolveSymbols(Program* program) {
    /**
      TODO: Full iterative res will be added here.
      For now, all labels are resolved in phase 1 so, do nothing.
      In the future, we will resolve EQU constants, handle forward references in expressions
    */

    return false;
}

/**
  99% of the bugs we will find in the future are related to this function.
*/
uint64_t SemanticAnalyzer::calculateInstructionSize(Instruction* instr) {
    std::string mnemonic = instr->mnemonic;
    std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), ::toupper);

    // No Operand - 1 byte
    if (instr->operands.empty()) {
        // String instructions
        if (mnemonic == "MOVSB" || mnemonic == "MOVSW" ||
            mnemonic == "CMPSB" || mnemonic == "CMPSW" ||
            mnemonic == "SCASB" || mnemonic == "SCASW" ||
            mnemonic == "LODSB" || mnemonic == "LODSW" ||
            mnemonic == "STOSB" || mnemonic == "STOSW") {
            return 1;
        }

        // Control flow
        if (mnemonic == "NOP" || mnemonic == "HLT" ||
            mnemonic == "RET" || mnemonic == "RETF" ||
            mnemonic == "IRET") {
            return 1;
        }

        // Stack operations
        if (mnemonic == "PUSHA" || mnemonic == "POPA" ||
            mnemonic == "PUSHF" || mnemonic == "POPF") {
            return 1;
        }

        // Flag operations
        if (mnemonic == "CLC" || mnemonic == "STC" || mnemonic == "CMC" ||
            mnemonic == "CLD" || mnemonic == "STD" ||
            mnemonic == "CLI" || mnemonic == "STI") {
            return 1;
        }

        // Conversion
        if (mnemonic == "CBW" || mnemonic == "CWD" ||
            mnemonic == "LAHF" || mnemonic == "SAHF") {
            return 1;
        }

        // BCD arithmetic
        if (mnemonic == "AAA" || mnemonic == "AAS" ||
            mnemonic == "AAM" || mnemonic == "AAD" ||
            mnemonic == "DAA" || mnemonic == "DAS") {
            return 1;
        }

        // Other
        if (mnemonic == "XLAT" || mnemonic == "WAIT" || mnemonic == "LOCK" ||
            mnemonic == "INT3" || mnemonic == "INTO") {
            return 1;
        }

        // Repeat prefixes
        if (mnemonic == "REP" || mnemonic == "REPE" || mnemonic == "REPZ" ||
            mnemonic == "REPNE" || mnemonic == "REPNZ") {
            return 1;
        }
    }

    // RET with imm16 - 3 bytes
    if ((mnemonic == "RET" || mnemonic == "RETF") && instr->operands.size() == 1) {
        return 3;  // Opcode (1) + imm16 (2)
    }

    // JMPs (My nightmare)
    // Im sure there are some bugs down there but they are hidden for now
    if (mnemonic == "JMP" || mnemonic == "CALL") {
        if (instr->operands.size() == 1) {
            auto* label_ref = dynamic_cast<LabelRef*>(instr->operands[0].get());
            if (label_ref) {
                // CALL must always be NEAR (no SHORT encoding)
                if (mnemonic == "CALL") {
                    return 3;  // NEAR: opcode (1) + rel16 (2)
                }

                // For JMP: Use jump_type set by parser (no optimization)
                // TODO: Implement multi-pass assembly for proper jump optimization
                // Single-pass optimization causes forward reference issues
                if (label_ref->jump_type == LabelRef::JumpType::SHORT) {
                    return 2;
                } else {
                    return 3;  // NEAR
                }
            }
            // Default to NEAR
            return 3;
        }
        // Indirect jumps (JMP reg or JMP [mem])
        return 2;  // Opcode (1) + ModRM (1)
    }

    // Conditional jumps (always SHORT - 2 bytes)
    if (mnemonic == "JE" || mnemonic == "JNE" || mnemonic == "JZ" || mnemonic == "JNZ" ||
        mnemonic == "JL" || mnemonic == "JLE" || mnemonic == "JG" || mnemonic == "JGE" ||
        mnemonic == "JNL" || mnemonic == "JNLE" || mnemonic == "JNG" || mnemonic == "JNGE" ||
        mnemonic == "JA" || mnemonic == "JAE" || mnemonic == "JB" || mnemonic == "JBE" ||
        mnemonic == "JNA" || mnemonic == "JNAE" || mnemonic == "JNB" || mnemonic == "JNBE" ||
        mnemonic == "JC" || mnemonic == "JNC" || mnemonic == "JO" || mnemonic == "JNO" ||
        mnemonic == "JS" || mnemonic == "JNS" || mnemonic == "JP" || mnemonic == "JPE" ||
        mnemonic == "JNP" || mnemonic == "JPO" ||
        mnemonic == "LOOP" || mnemonic == "LOOPE" || mnemonic == "LOOPZ" ||
        mnemonic == "LOOPNE" || mnemonic == "LOOPNZ" || mnemonic == "JCXZ") {
        return 2;  // Opcode (1) + rel8 (1)
    }

    // INT instruction
    if (mnemonic == "INT" && instr->operands.size() == 1) {
        return 2;  // Opcode (1) + imm8 (1)
    }

    // I/O INSTRUCTIONS
    if (mnemonic == "IN" || mnemonic == "OUT") {
        if (instr->operands.size() == 2) {
            // Check if using immediate port or DX
            auto* imm = dynamic_cast<ImmediateOperand*>(instr->operands[0].get());
            if (!imm) imm = dynamic_cast<ImmediateOperand*>(instr->operands[1].get());

            if (imm) {
                return 2;  // Opcode (1) + imm8 (1)
            }
            return 1;  // Using DX register
        }
    }

    // MOV INSTRUCTION
    if (mnemonic == "MOV" && instr->operands.size() == 2) {
        auto* dest_reg = dynamic_cast<RegisterOperand*>(instr->operands[0].get());
        auto* src_reg = dynamic_cast<RegisterOperand*>(instr->operands[1].get());
        auto* dest_mem = dynamic_cast<MemoryOperand*>(instr->operands[0].get());
        auto* src_mem = dynamic_cast<MemoryOperand*>(instr->operands[1].get());
        auto* imm = dynamic_cast<ImmediateOperand*>(instr->operands[1].get());

        // MOV reg, immediate - use B8+r encoding (3 bytes for 16-bit, 2 for 8-bit)
        if (dest_reg && imm) {
            if (dest_reg->size == 16) {
                return 3;  // Opcode (1) + imm16 (2)
            } else {
                return 2;  // Opcode (1) + imm8 (1)
            }
        }

        // MOV reg, reg - 2 bytes (opcode + ModRM)
        if (dest_reg && src_reg) {
            return 2;
        }

        // MOV with memory operands
        if (dest_mem || src_mem) {
            const MemoryOperand* mem = dest_mem ? dest_mem : src_mem;
            uint64_t seg_prefix = mem->segment_override.has_value() ? 1 : 0;

            // MOV mem, imm - needs opcode + ModRM + displacement + immediate
            if ((dest_mem && imm)) {
                uint64_t mem_size = calculateMemoryEncodingSize(dest_mem);
                uint64_t imm_size = (dest_mem->size_hint == 16 || (dest_mem->size_hint == 0 && imm->value > 255)) ? 2 : 1;
                return seg_prefix + 1 + mem_size + imm_size;  // prefix + opcode + mem_encoding + imm
            }
            // MOV AX/AL, [moffs] or MOV [moffs], AX/AL uses special 3-byte encoding
            // But only for direct addresses (no registers)
            if ((dest_reg && dest_reg->code == 0 && src_mem) ||
                (src_reg && src_reg->code == 0 && dest_mem)) {
                // Check if it's a direct address (moffs encoding)
                bool is_moffs = mem->is_direct_address ||
                    (mem->parsed_address && mem->parsed_address->registers.empty());
                if (is_moffs) {
                    return seg_prefix + 3;  // prefix + opcode (1) + moffs16 (2)
                }
            }
            // MOV reg, mem or MOV mem, reg (general form)
            uint64_t mem_size = calculateMemoryEncodingSize(mem);
            return seg_prefix + 1 + mem_size;  // prefix + opcode + mem_encoding
        }
    }

    // PUSH/POP
    if (mnemonic == "PUSH" || mnemonic == "POP") {
        if (instr->operands.size() == 1) {
            auto* reg = dynamic_cast<RegisterOperand*>(instr->operands[0].get());
            if (reg) {
                return 1;  // PUSH/POP reg uses single-byte encoding
            }
            // PUSH/POP memory
            return 2;  // Opcode + ModRM
        }
    }

    // INC/DEC
    if (mnemonic == "INC" || mnemonic == "DEC") {
        if (instr->operands.size() == 1) {
            auto* reg = dynamic_cast<RegisterOperand*>(instr->operands[0].get());
            if (reg && reg->size == 16) {
                return 1;  // Short form for 16-bit registers
            }
            // Memory operand or 8-bit register
            auto* mem = dynamic_cast<MemoryOperand*>(instr->operands[0].get());
            if (mem) {
                uint64_t seg_prefix = mem->segment_override.has_value() ? 1 : 0;
                return seg_prefix + 1 + calculateMemoryEncodingSize(mem);  // prefix + opcode + mem_encoding
            }
            // 8-bit register
            return 2;  // Opcode + ModRM
        }
    }

    // ARITHMETIC/LOGIC WITH IMMEDIATE
    if (mnemonic == "ADD" || mnemonic == "ADC" || mnemonic == "SUB" || mnemonic == "SBB" ||
        mnemonic == "CMP" || mnemonic == "AND" || mnemonic == "OR" || mnemonic == "XOR") {

        if (instr->operands.size() == 2) {
            auto* reg = dynamic_cast<RegisterOperand*>(instr->operands[0].get());
            auto* imm = dynamic_cast<ImmediateOperand*>(instr->operands[1].get());

            // AL/AX with immediate (special 2-byte encoding for AX, 3 bytes total)
            if (reg && reg->code == 0 && imm) {
                if (reg->size == 16) {
                    return 3;  // Opcode (1) + imm16 (2)
                } else {
                    return 2;  // Opcode (1) + imm8 (1)
                }
            }

            // General form with immediate
            if (imm) {
                // Check if first operand is memory
                auto* mem = dynamic_cast<MemoryOperand*>(instr->operands[0].get());
                if (mem) {
                    uint64_t seg_prefix = mem->segment_override.has_value() ? 1 : 0;
                    uint64_t mem_size = calculateMemoryEncodingSize(mem);
                    uint64_t imm_size = (mem->size_hint == 16) ? 2 : 1;
                    return seg_prefix + 1 + mem_size + imm_size;  // prefix + opcode + mem_encoding + imm
                } else if (reg && reg->size == 16) {
                    // Check if immediate has byte size hint (sign-extended imm8)
                    if (imm->size_hint == 8) {
                        return 3;  // Opcode (1) + ModRM (1) + imm8 (1) - uses 83 /r ib encoding
                    }
                    return 4;  // Opcode (1) + ModRM (1) + imm16 (2)
                } else {
                    return 3;  // Opcode (1) + ModRM (1) + imm8 (1)
                }
            }

            // reg, reg
            if (instr->operands.size() == 2) {
                auto* op0_reg = dynamic_cast<RegisterOperand*>(instr->operands[0].get());
                auto* op1_reg = dynamic_cast<RegisterOperand*>(instr->operands[1].get());
                if (op0_reg && op1_reg) {
                    return 2;  // Opcode + ModRM
                }
            }

            // reg, mem or mem, reg
            auto* mem0 = dynamic_cast<MemoryOperand*>(instr->operands[0].get());
            auto* mem1 = dynamic_cast<MemoryOperand*>(instr->operands[1].get());
            const MemoryOperand* mem = mem0 ? mem0 : mem1;
            if (mem) {
                uint64_t seg_prefix = mem->segment_override.has_value() ? 1 : 0;
                return seg_prefix + 1 + calculateMemoryEncodingSize(mem);  // prefix + opcode + mem_encoding
            }
            return 4;  // Fallback
        }
    }

    // TEST
    if (mnemonic == "TEST" && instr->operands.size() == 2) {
        auto* reg = dynamic_cast<RegisterOperand*>(instr->operands[0].get());
        auto* imm = dynamic_cast<ImmediateOperand*>(instr->operands[1].get());

        if (reg && reg->code == 0 && imm) {
            // TEST AL/AX, imm
            return (reg->size == 16) ? 3 : 2;
        }
        if (imm) {
            // TEST r/m, imm
            return (reg && reg->size == 16) ? 4 : 3;
        }
        return 2;  // TEST r/m, reg
    }

    // SHIFTS/ROTATES
    if (mnemonic == "ROL" || mnemonic == "ROR" || mnemonic == "RCL" || mnemonic == "RCR" ||
        mnemonic == "SHL" || mnemonic == "SHR" || mnemonic == "SAL" || mnemonic == "SAR") {

        if (instr->operands.size() == 1) {
            return 2;  // Shift by 1 (implicit): opcode + ModRM
        }
        if (instr->operands.size() == 2) {
            auto* cl_reg = dynamic_cast<RegisterOperand*>(instr->operands[1].get());
            if (cl_reg && cl_reg->code == 1) {
                return 2;  // Shift by CL: opcode + ModRM
            }
            return 2;  // Shift by 1 (explicit): opcode + ModRM
        }
    }

    // UNARY OPERATIONS
    if (mnemonic == "NOT" || mnemonic == "NEG" ||
        mnemonic == "MUL" || mnemonic == "IMUL" ||
        mnemonic == "DIV" || mnemonic == "IDIV") {
        return 2;  // Opcode + ModRM
    }

    // LEA, LDS, LES
    if (mnemonic == "LEA" || mnemonic == "LDS" || mnemonic == "LES") {
        if (instr->operands.size() >= 2) {
            auto* mem = dynamic_cast<MemoryOperand*>(instr->operands[1].get());
            if (mem) {
                uint64_t seg_prefix = mem->segment_override.has_value() ? 1 : 0;
                return seg_prefix + 1 + calculateMemoryEncodingSize(mem);  // prefix + opcode + mem_encoding
            }
        }
        return 4;  // Fallback
    }

    // XCHG
    if (mnemonic == "XCHG" && instr->operands.size() == 2) {
        auto* reg1 = dynamic_cast<RegisterOperand*>(instr->operands[0].get());
        auto* reg2 = dynamic_cast<RegisterOperand*>(instr->operands[1].get());

        // XCHG AX, reg or XCHG reg, AX
        if ((reg1 && reg1->code == 0 && reg1->size == 16) ||
            (reg2 && reg2->code == 0 && reg2->size == 16)) {
            return 1;  // Short form
        }
        return 2;  // General form: opcode + ModRM
    }

    /**
      This is also garbage, I should return 0 here because the perfect scenario is
      having a single source of truth for know/unknown instructions (maybe it should be the encoding table)
      but for now, it defaults to 3 to mitigate any instruction that the lexer will accept but not handled here
      like, for example, STOS/LODS/SCAS(B/W).
      Defaulting to anything also adds mainenance. I'm sure in the future I will change something inside
      s_instructions and I will forget to change it here and I will debug for days to find out that
      I deserved the pain because I chosed not to fix the design just because I'm lazy.
    */
    return 3;
}

uint64_t SemanticAnalyzer::calculateDataSize(const std::string& directive, size_t value_count) {
    if (directive == "DB") return value_count * 1;
    if (directive == "DW") return value_count * 2;
    if (directive == "DD") return value_count * 4;
    if (directive == "DQ") return value_count * 8;
    if (directive == "DT") return value_count * 10;
    return 0;
}

uint64_t SemanticAnalyzer::calculateMemoryEncodingSize(const MemoryOperand* mem) {
    // Returns the size of ModRM byte + displacement bytes
    // Does NOT include segment prefix (caller must add 1 if segment_override is present)

    if (!mem) return 3;  // Conservative fallback

    // Direct numeric address: ModRM (1) + disp16 (2)
    if (mem->is_direct_address) {
        return 3;
    }

    // Check parsed address for register indirect vs direct
    if (mem->parsed_address) {
        const auto& addr = *mem->parsed_address;

        // Direct address with label: ModRM (1) + disp16 (2)
        if (addr.registers.empty()) {
            return 3;
        }

        // Register indirect with no displacement
        // Special case: [BP] alone requires at least disp8
        if (!addr.has_displacement && !addr.has_label) {
            if (addr.registers.size() == 1 && addr.registers[0] == "BP") {
                return 2;  // ModRM (1) + disp8 (1)
            }
            return 1;  // Just ModRM, no displacement
        }

        // Has displacement - check if it fits in 8 bits
        int64_t disp = addr.displacement;
        if (addr.has_label) {
            // Label reference - assume 16-bit displacement needed
            return 3;  // ModRM (1) + disp16 (2)
        }

        if (disp >= -128 && disp <= 127) {
            return 2;  // ModRM (1) + disp8 (1)
        }
        return 3;  // ModRM (1) + disp16 (2)
    }

    // Fallback: assume direct addressing with disp16
    return 3;
}

std::optional<uint64_t> SemanticAnalyzer::getAddress(size_t statement_index) const {
    for (const auto& addr_info : m_addresses) {
        if (addr_info.statement_index == statement_index) {
            return addr_info.address;
        }
    }
    return std::nullopt;
}

void SemanticAnalyzer::error(const std::string& message, SourceLocation loc) {
    m_errors.push_back(Error(message, loc, ErrorSeverity::ERROR));
}

void SemanticAnalyzer::setOrigin(uint64_t address) {
    m_origin_address = address;
    m_current_address = address;
    m_segment_start_address = address;
}

void SemanticAnalyzer::enterSegment(const std::string& name) {
    // Warn if transitioning from code to data without a terminator
    if (!m_current_segment.empty() &&
        isCodeSegment(m_current_segment) &&
        isDataSegment(name) &&
        !m_last_was_terminator) {
        error("Warning: Code segment '" + m_current_segment +
              "' may fall through into data segment '" + name +
              "'. Consider adding HLT, JMP, or RET before the data section.",
              SourceLocation());
    }

    // Reset terminator flag when entering new segment
    m_last_was_terminator = false;

    // Check if segment already exists
    for (auto& seg : m_segments) {
        if (seg.name == name) {
            // Switch to existing segment
            m_current_segment = name;
            m_current_address = seg.current_address;
            m_segment_start_address = seg.start_address;
            return;
        }
    }

    // Create new segment
    SegmentInfo seg;
    seg.name = name;
    seg.start_address = m_current_address;
    seg.current_address = m_current_address;
    m_segments.push_back(seg);

    m_current_segment = name;
    m_segment_start_address = m_current_address;
}

void SemanticAnalyzer::exitSegment(const std::string& name) {
    // Update segment's current address
    for (auto& seg : m_segments) {
        if (seg.name == name || (name.empty() && seg.name == m_current_segment)) {
            seg.current_address = m_current_address;
            break;
        }
    }

    // Don't switch segments - just mark segment as closed
    // m_current_segment.clear();  // Keep current segment active??
}

bool SemanticAnalyzer::isCodeSegment(const std::string& name) const {
    // Common code segment names
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    return lower_name == ".text" || lower_name == "text" ||
           lower_name == ".code" || lower_name == "code" ||
           lower_name == "_text" || lower_name == "_code";
}

bool SemanticAnalyzer::isDataSegment(const std::string& name) const {
    // Common data segment names
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    return lower_name == ".data" || lower_name == "data" ||
           lower_name == ".bss" || lower_name == "bss" ||
           lower_name == ".rodata" || lower_name == "rodata" ||
           lower_name == "_data" || lower_name == "_bss";
}

bool SemanticAnalyzer::resolveSymbol(const std::string& name, SourceLocation loc, int64_t& out_value) {
    auto symbol = m_symbol_table.lookup(name);
    if (!symbol) {
        error("Undefined symbol: " + name, loc);
        return false;
    }
    if (!symbol->is_resolved) {
        error("Symbol '" + name + "' is not yet resolved", loc);
        return false;
    }
    out_value = symbol->value;
    return true;
}

bool SemanticAnalyzer::resolveDataSymbols(DataDirective* data) {
    for (auto& value : data->values) {
        if (value.type == DataValue::Type::SYMBOL) {
            int64_t resolved_value;
            if (!resolveSymbol(value.string_value, data->location, resolved_value)) {
                return false;
            }
            // Convert SYMBOL to NUMBER with resolved value
            value.number_value = resolved_value;
            value.type = DataValue::Type::NUMBER;
        }
    }
    return true;
}

std::function<std::optional<int64_t>(const std::string&)> SemanticAnalyzer::createSymbolLookup() {
    return [this](const std::string& name) -> std::optional<int64_t> {
        auto symbol = m_symbol_table.lookup(name);
        if (symbol && symbol->is_resolved) {
            return symbol->value;
        }
        return std::nullopt;
    };
}

bool SemanticAnalyzer::resolveMemoryOperands(Instruction* instr) {
    auto symbol_lookup = createSymbolLookup();

    for (auto& operand : instr->operands) {
        if (auto* mem = dynamic_cast<MemoryOperand*>(operand.get())) {
            // Re-parse the address expression with symbol resolution
            auto parsed = ExpressionParser::parseAddressWithSymbols(
                mem->address_expr, symbol_lookup
            );

            if (!parsed) {
                error("Invalid memory operand: " + mem->address_expr, mem->location);
                return false;
            }

            // Update the parsed address
            mem->parsed_address = std::make_unique<AddressExpression>(*parsed);

            // Check if it's a direct address (no registers)
            if (parsed->registers.empty() && !parsed->has_label) {
                mem->is_direct_address = true;
                mem->direct_address_value = static_cast<uint16_t>(parsed->displacement);
            }
        }
    }

    return true;
}

} // namespace e2asm
