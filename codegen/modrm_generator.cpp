#include "modrm_generator.h"
#include "../parser/ast.h"
#include "../semantic/symbol_table.h"
#include <algorithm>

namespace e2asm {

const std::unordered_map<std::string, uint8_t> ModRMGenerator::RM_CODES = {
    {"BX+SI", 0x00},
    {"BX+DI", 0x01},
    {"BP+SI", 0x02},
    {"BP+DI", 0x03},
    {"SI", 0x04},
    {"DI", 0x05},
    {"BP", 0x06},
    {"BX", 0x07},
    {"DA", 0x06}  // Direct address (special case)
};

uint8_t ModRMGenerator::generateRegToReg(uint8_t reg_field, uint8_t rm_field) {
    // Register-to-register: MOD = 11b (0x03)
    return combineModRM(0x03, reg_field, rm_field);
}

ModRMResult ModRMGenerator::generateMemory(const AddressExpression& addr_expr, uint8_t reg_field,
                                           const SymbolTable* symbol_table) {
    // Resolve label if present
    int64_t total_displacement = addr_expr.displacement;
    bool has_disp = addr_expr.has_displacement;

    if (addr_expr.has_label) {
        if (!symbol_table) {
            return ModRMResult("Symbol table not available for label resolution");
        }
        auto symbol = symbol_table->lookup(addr_expr.label_name);
        if (!symbol || !symbol->is_resolved) {
            return ModRMResult("Undefined label: " + addr_expr.label_name);
        }
        total_displacement += symbol->value;
        has_disp = true;
    }

    // Determine R/M code from registers
    auto rm_code = calculateRM(addr_expr.registers);
    if (!rm_code) {
        return ModRMResult("Invalid addressing mode combination");
    }

    // Special case: Direct address (no registers, just displacement/label)
    // Must use MOD=00, R/M=110, 16-bit displacement
    if (addr_expr.registers.empty() && (has_disp || addr_expr.has_label)) {
        uint8_t modrm = combineModRM(0x00, reg_field, 0x06);
        std::vector<uint8_t> disp_bytes = encodeDisplacement(total_displacement, 2);
        return ModRMResult(modrm, disp_bytes);
    }

    // Calculate MOD field based on displacement
    uint8_t mod = calculateMod(total_displacement, has_disp);

    // Special case: [BP] without displacement requires MOD=01 with disp8=0
    if (addr_expr.registers.size() == 1 && addr_expr.registers[0] == "BP" && !has_disp) {
        mod = 0x01;  // Force 8-bit displacement
        uint8_t modrm = combineModRM(mod, reg_field, *rm_code);
        return ModRMResult(modrm, std::vector<uint8_t>{0x00});
    }

    // Generate ModRM byte
    uint8_t modrm = combineModRM(mod, reg_field, *rm_code);

    // Generate displacement bytes
    std::vector<uint8_t> disp_bytes;
    if (mod == 0x01) {
        // 8-bit displacement
        disp_bytes = encodeDisplacement(total_displacement, 1);
    } else if (mod == 0x02) {
        // 16-bit displacement
        disp_bytes = encodeDisplacement(total_displacement, 2);
    }
    // mod == 0x00: no displacement

    return ModRMResult(modrm, disp_bytes);
}

ModRMResult ModRMGenerator::generateDirect(uint16_t address, uint8_t reg_field) {
    // Direct addressing: MOD=00, R/M=110 (special case)
    uint8_t modrm = combineModRM(0x00, reg_field, 0x06);

    // Direct address is always 16-bit
    std::vector<uint8_t> disp_bytes = encodeDisplacement(address, 2);

    return ModRMResult(modrm, disp_bytes);
}

uint8_t ModRMGenerator::calculateMod(int64_t displacement, bool has_displacement) {
    if (!has_displacement) {
        return 0x00;
    }

    // Check if displacement fits in 8-bit signed
    if (isMod1(displacement)) {
        return 0x01;  // 8-bit displacement
    }

    return 0x02;  // 16-bit displacement
}

std::optional<uint8_t> ModRMGenerator::calculateRM(const std::vector<std::string>& registers) {
    if (registers.empty()) {
        // Direct address
        return 0x06;
    }

    if (registers.size() == 1) {
        // Single register
        const std::string& reg = registers[0];

        if (reg == "BX") return RM_CODES.at("BX");
        if (reg == "SI") return RM_CODES.at("SI");
        if (reg == "DI") return RM_CODES.at("DI");
        if (reg == "BP") return RM_CODES.at("BP");

        return std::nullopt;  // Invalid register
    }

    if (registers.size() == 2) {
        // Two registers - check valid combinations
        bool has_bx = std::find(registers.begin(), registers.end(), "BX") != registers.end();
        bool has_bp = std::find(registers.begin(), registers.end(), "BP") != registers.end();
        bool has_si = std::find(registers.begin(), registers.end(), "SI") != registers.end();
        bool has_di = std::find(registers.begin(), registers.end(), "DI") != registers.end();

        if (has_bx && has_si) return RM_CODES.at("BX+SI");
        if (has_bx && has_di) return RM_CODES.at("BX+DI");
        if (has_bp && has_si) return RM_CODES.at("BP+SI");
        if (has_bp && has_di) return RM_CODES.at("BP+DI");

        return std::nullopt;  // Invalid combination
    }

    // More than 2 registers - invalid
    return std::nullopt;
}

bool ModRMGenerator::isMod1(int64_t displacement) {
    // Check if displacement fits in 8-bit signed range
    return (displacement >= -128 && displacement <= 127);
}

std::vector<uint8_t> ModRMGenerator::encodeDisplacement(int64_t value, size_t size_bytes) {
    std::vector<uint8_t> bytes;

    // Little-endian encoding
    for (size_t i = 0; i < size_bytes; i++) {
        bytes.push_back(static_cast<uint8_t>(value & 0xFF));
        value >>= 8;
    }

    return bytes;
}

uint8_t ModRMGenerator::combineModRM(uint8_t mod, uint8_t reg, uint8_t rm) {
    return ((mod & 0x03) << 6) | ((reg & 0x07) << 3) | (rm & 0x07);
}

} // namespace e2asm
