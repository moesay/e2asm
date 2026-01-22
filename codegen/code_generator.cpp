#include "code_generator.h"
#include <sstream>
#include <iomanip>
#include <iostream>

namespace e2asm {

CodeGenerator::CodeGenerator()
    : m_current_address(0)
{
}

AssemblyResult CodeGenerator::generate(const Program* program) {
    AssemblyResult result;
    m_binary.clear();
    m_listing.clear();
    m_current_address = 0;
    m_error_reporter.clear();

    Program* non_const_program = const_cast<Program*>(program);
    if (!m_semantic_analyzer.analyze(non_const_program)) {
        result.errors = m_semantic_analyzer.getErrors();
        result.success = false;
        return result;
    }

    m_current_address = m_semantic_analyzer.getOriginAddress();

    m_encoder.setSymbolTable(&m_semantic_analyzer.getSymbolTable());

    for (const auto& stmt : program->statements) {
        if (!generateStatement(stmt.get())) {
            break;
        }
    }

    result.binary = m_binary;
    result.listing = m_listing;
    result.errors = m_error_reporter.getErrors();
    result.success = !m_error_reporter.hasErrors();
    result.origin_address = m_semantic_analyzer.getOriginAddress();

    for (const auto& [key, symbol] : m_semantic_analyzer.getSymbolTable().getAllSymbols()) {
        if (symbol.type == SymbolType::LABEL) {
            result.symbols[symbol.name] = symbol.value;
        }
    }

    return result;
}

bool CodeGenerator::generateStatement(const ASTNode* stmt) {
    if (auto* label = dynamic_cast<const Label*>(stmt)) {
        processLabel(label);
        return true;
    }
    else if (auto* instr = dynamic_cast<const Instruction*>(stmt)) {
        return processInstruction(instr);
    }
    else if (auto* data = dynamic_cast<const DataDirective*>(stmt)) {
        return processDataDirective(data);
    }
    else if (auto* equ = dynamic_cast<const EQUDirective*>(stmt)) {
        processEQUDirective(equ);
        return true;
    }
    else if (auto* org = dynamic_cast<const ORGDirective*>(stmt)) {
        processORGDirective(org);
        return true;
    }
    else if (auto* seg = dynamic_cast<const SEGMENTDirective*>(stmt)) {
        processSEGMENTDirective(seg);
        return true;
    }
    else if (auto* ends = dynamic_cast<const ENDSDirective*>(stmt)) {
        processENDSDirective(ends);
        return true;
    }
    else if (auto* res = dynamic_cast<const RESDirective*>(stmt)) {
        return processRESDirective(res);
    }
    else if (auto* times = dynamic_cast<const TIMESDirective*>(stmt)) {
        return processTIMESDirective(times);
    }

    // Unknown statement type
    return true;
}

void CodeGenerator::processLabel(const Label* label) {
    if (!SymbolTable::isLocalLabel(label->name)) {
        m_semantic_analyzer.getSymbolTable().setGlobalScope(label->name);
    }

    AssembledLine line;
    line.source_line = label->location.line;
    line.source_text = label->name + ":";
    line.address = m_current_address;
    line.success = true;

    m_listing.push_back(line);
}

bool CodeGenerator::processInstruction(const Instruction* instr) {
    // CRITICAL: Use assigned_address from semantic analysis for jump calculations
    // The symbol table has addresses based on semantic analysis estimates,
    // so we must use the same address system for consistency
    m_encoder.setCurrentAddress(instr->assigned_address);

    auto encoded = m_encoder.encode(instr);

    AssembledLine line;
    line.source_line = instr->location.line;
    line.address = m_current_address;

    std::ostringstream source;
    source << instr->mnemonic;
    for (size_t i = 0; i < instr->operands.size(); i++) {
        if (i == 0) source << " ";
        else source << ", ";

        const auto& op = instr->operands[i];

        if (auto* reg = dynamic_cast<RegisterOperand*>(op.get())) {
            source << reg->name;
        }
        else if (auto* imm = dynamic_cast<ImmediateOperand*>(op.get())) {
            source << "0x" << std::hex << imm->value << std::dec;
        }
        else if (auto* mem = dynamic_cast<MemoryOperand*>(op.get())) {
            source << "[" << mem->address_expr << "]";
        }
    }
    line.source_text = source.str();

    if (encoded.success) {
        line.machine_code = encoded.bytes;
        line.success = true;

        m_binary.insert(m_binary.end(), encoded.bytes.begin(), encoded.bytes.end());
        m_current_address += encoded.bytes.size();
    } else {
        line.success = false;
        line.error_message = encoded.error;

        m_error_reporter.error(encoded.error, instr->location);
    }

    m_listing.push_back(line);
    return encoded.success;
}

bool CodeGenerator::processDataDirective(const DataDirective* directive) {
    AssembledLine line;
    line.source_line = directive->location.line;
    line.address = m_current_address;
    line.success = true;

    std::ostringstream source;
    switch (directive->size) {
        case DataDirective::Size::BYTE: source << "DB "; break;
        case DataDirective::Size::WORD: source << "DW "; break;
        case DataDirective::Size::DWORD: source << "DD "; break;
        case DataDirective::Size::QWORD: source << "DQ "; break;
        case DataDirective::Size::TBYTE: source << "DT "; break;
    }

    size_t element_size = 0;
    switch (directive->size) {
        case DataDirective::Size::BYTE: element_size = 1; break;
        case DataDirective::Size::WORD: element_size = 2; break;
        case DataDirective::Size::DWORD: element_size = 4; break;
        case DataDirective::Size::QWORD: element_size = 8; break;
        case DataDirective::Size::TBYTE: element_size = 10; break;
    }

    for (size_t i = 0; i < directive->values.size(); i++) {
        const auto& value = directive->values[i];

        if (i > 0) source << ", ";

        if (value.type == DataValue::Type::STRING) {
            // String - emit as bytes
            source << "\"" << value.string_value << "\"";
            for (char c : value.string_value) {
                line.machine_code.push_back(static_cast<uint8_t>(c));
                m_binary.push_back(static_cast<uint8_t>(c));
            }
            m_current_address += value.string_value.length();
        }
        else if (value.type == DataValue::Type::CHARACTER) {
            // Character - emit as byte
            source << "'" << value.string_value << "'";
            if (!value.string_value.empty()) {
                line.machine_code.push_back(static_cast<uint8_t>(value.string_value[0]));
                m_binary.push_back(static_cast<uint8_t>(value.string_value[0]));
                m_current_address += 1;
            }
        }
        else {
            // Number - emit in little-endian
            source << "0x" << std::hex << value.number_value << std::dec;

            int64_t num = value.number_value;
            for (size_t j = 0; j < element_size; j++) {
                uint8_t byte = static_cast<uint8_t>(num & 0xFF);
                line.machine_code.push_back(byte);
                m_binary.push_back(byte);
                num >>= 8;
            }
            m_current_address += element_size;
        }
    }

    line.source_text = source.str();
    m_listing.push_back(line);
    return true;
}

void CodeGenerator::processEQUDirective(const EQUDirective* directive) {
    AssembledLine line;
    line.source_line = directive->location.line;
    line.source_text = directive->name + " EQU " + std::to_string(directive->value);
    line.address = m_current_address;
    line.success = true;

    m_listing.push_back(line);
}

void CodeGenerator::processORGDirective(const ORGDirective* directive) {
    AssembledLine line;
    line.source_line = directive->location.line;
    line.source_text = "ORG 0x" + std::to_string(directive->address);
    line.address = m_current_address;
    line.success = true;

    m_listing.push_back(line);
}

void CodeGenerator::processSEGMENTDirective(const SEGMENTDirective* directive) {
    AssembledLine line;
    line.source_line = directive->location.line;
    line.source_text = "SEGMENT " + directive->name;
    line.address = m_current_address;
    line.success = true;

    m_listing.push_back(line);
}

void CodeGenerator::processENDSDirective(const ENDSDirective* directive) {
    AssembledLine line;
    line.source_line = directive->location.line;
    line.source_text = directive->name.empty() ? "ENDS" : directive->name + " ENDS";
    line.address = m_current_address;
    line.success = true;

    m_listing.push_back(line);
}

bool CodeGenerator::processRESDirective(const RESDirective* directive) {
    AssembledLine line;
    line.source_line = directive->location.line;
    line.address = m_current_address;
    line.success = true;

    std::string res_name;
    switch (directive->size) {
        case RESDirective::Size::BYTE: res_name = "RESB"; break;
        case RESDirective::Size::WORD: res_name = "RESW"; break;
        case RESDirective::Size::DWORD: res_name = "RESD"; break;
        case RESDirective::Size::QWORD: res_name = "RESQ"; break;
        case RESDirective::Size::TBYTE: res_name = "REST"; break;
    }
    line.source_text = res_name + " " + std::to_string(directive->count);

    size_t element_size = 0;
    switch (directive->size) {
        case RESDirective::Size::BYTE: element_size = 1; break;
        case RESDirective::Size::WORD: element_size = 2; break;
        case RESDirective::Size::DWORD: element_size = 4; break;
        case RESDirective::Size::QWORD: element_size = 8; break;
        case RESDirective::Size::TBYTE: element_size = 10; break;
    }

    size_t total_size = element_size * directive->count;

    for (size_t i = 0; i < total_size; i++) {
        line.machine_code.push_back(0x00);
        m_binary.push_back(0x00);
    }

    m_current_address += total_size;
    m_listing.push_back(line);
    return true;
}

bool CodeGenerator::processTIMESDirective(const TIMESDirective* directive) {
    for (int64_t i = 0; i < directive->count; i++) {
        if (!generateStatement(directive->repeated_node.get())) {
            return false;
        }
    }
    return true;
}

} // namespace e2asm
