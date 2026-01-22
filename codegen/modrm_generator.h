#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include "../parser/expression_parser.h"

namespace e2asm {

struct ModRMResult {
    uint8_t modrm_byte;
    std::vector<uint8_t> displacement;
    bool success;
    std::string error;

    ModRMResult() : modrm_byte(0), success(false) {}
    ModRMResult(uint8_t modrm) : modrm_byte(modrm), success(true) {}
    ModRMResult(uint8_t modrm, std::vector<uint8_t> disp)
        : modrm_byte(modrm), displacement(std::move(disp)), success(true) {}
    ModRMResult(std::string err) : modrm_byte(0), success(false), error(std::move(err)) {}
};

class ModRMGenerator {
public:
    /**
     * Generate ModRM byte for register-to-register
     * @param reg_field REG field (0-7)
     * @param rm_field R/M field (0-7)
     * @return ModRM byte with MOD=11
     */
    static uint8_t generateRegToReg(uint8_t reg_field, uint8_t rm_field);

    /**
     * Generate ModRM byte + displacement for memory operand
     * @param addr_expr Parsed address expression
     * @param reg_field REG field (0-7)
     * @param symbol_table Symbol table for resolving labels (optional)
     * @return ModRM byte and displacement bytes
     */
    static ModRMResult generateMemory(const AddressExpression& addr_expr, uint8_t reg_field,
                                     const class SymbolTable* symbol_table = nullptr);

    /**
     * Generate ModRM byte + displacement for direct memory address
     * @param address Direct memory address (e.g., [0x1000])
     * @param reg_field REG field (0-7)
     * @return ModRM byte and displacement bytes
     */
    static ModRMResult generateDirect(uint16_t address, uint8_t reg_field);

private:
    static uint8_t calculateMod(int64_t displacement, bool has_displacement);

    static std::optional<uint8_t> calculateRM(const std::vector<std::string>& registers);

    static bool isMod1(int64_t displacement);

    static std::vector<uint8_t> encodeDisplacement(int64_t value, size_t size_bytes);

    static uint8_t combineModRM(uint8_t mod, uint8_t reg, uint8_t rm);

    static const std::unordered_map<std::string, uint8_t> RM_CODES;
};

} // namespace e2asm
