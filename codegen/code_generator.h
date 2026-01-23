/**
 * @file code_generator.h
 * @brief Final compilation phase that emits 8086 machine code
 *
 * The code generator is the final phase. It walks the analyzed AST and produces
 * the actual machine code bytes, maintaining a detailed listing that maps source
 * lines to their binary encoding.
 */

#pragma once

#include <vector>
#include <cstdint>
#include "../parser/ast.h"
#include "../core/assembler.h"
#include "../core/error.h"
#include "../semantic/semantic_analyzer.h"
#include "instruction_encoder.h"

namespace e2asm {

/**
 * @brief Converts analyzed AST into 8086 machine code
 *
 * The code generator traverses the AST produced by the parser and validated
 * by the semantic analyzer, emitting machine code bytes for each statement.
 *
 * It handles:
 * - **Instructions**: Delegates to InstructionEncoder for complex 8086 encoding
 * - **Data directives**: Emits bytes, words, dwords as specified
 * - **Labels**: Records in the listing (no code emitted)
 * - **Directives**: Processes ORG, SEGMENT, TIMES, RES, etc.
 *
 * Maintains two outputs:
 * 1. Binary vector: Raw machine code bytes ready for execution
 * 2. Listing: Human-readable mapping of source lines to their encodings
 *
 * The generator performs semantic analysis as its first step, so clients
 * just need to pass a valid AST - symbol resolution happens automatically.
 */
class CodeGenerator {
public:
    CodeGenerator();

    /**
     * @brief Generates machine code from an AST
     * @param program Parsed and validated AST
     * @return Complete assembly result with binary, listing, symbols, and any errors
     *
     * This is the main entry point. It runs semantic analysis first to resolve
     * symbols and assign addresses, then walks the AST emitting code. Even if
     * errors occur, partial results may be available in the listing.
     */
    AssemblyResult generate(const Program* program);

private:
    /**
     * @brief Processes a single AST node and emits code
     * @param stmt Statement to process
     * @return true if code was generated successfully
     *
     * Dispatches to specific handlers based on statement type.
     */
    bool generateStatement(const ASTNode* stmt);

    /**
     * @brief Records a label in the listing
     * @param label Label node to process
     *
     * Labels don't emit code, but appear in the listing showing their address.
     */
    void processLabel(const Label* label);

    /**
     * @brief Encodes an instruction to machine code
     * @param instr Instruction to encode
     * @return true if encoding succeeded
     *
     * Delegates to InstructionEncoder which handles ModR/M byte generation
     * and all the encoding variants.
     */
    bool processInstruction(const Instruction* instr);

    /**
     * @brief Emits data bytes from DB/DW/DD/DQ/DT directive
     * @param directive Data directive to process
     * @return true if data was emitted successfully
     *
     * Converts numbers and strings to bytes in the appropriate width.
     */
    bool processDataDirective(const DataDirective* directive);

    /**
     * @brief Records an EQU constant (no code emitted)
     * @param directive EQU directive to process
     *
     * Constants appear in the symbol table but don't generate code.
     */
    void processEQUDirective(const EQUDirective* directive);

    /**
     * @brief Handles ORG directive
     * @param directive ORG directive to process
     *
     * Updates the address counter but doesn't emit bytes.
     */
    void processORGDirective(const ORGDirective* directive);

    /**
     * @brief Enters a segment
     * @param directive SEGMENT directive to process
     *
     * Starts a new logical section in the output.
     */
    void processSEGMENTDirective(const SEGMENTDirective* directive);

    /**
     * @brief Exits a segment
     * @param directive ENDS directive to process
     *
     * Closes the current segment.
     */
    void processENDSDirective(const ENDSDirective* directive);

    /**
     * @brief Reserves uninitialized space
     * @param directive RES directive to process
     * @return true if space was reserved successfully
     *
     * Advances the address counter and emits zero bytes or padding.
     */
    bool processRESDirective(const RESDirective* directive);

    /**
     * @brief Repeats a statement multiple times
     * @param directive TIMES directive to process
     * @return true if repetition succeeded
     *
     * Generates code for the repeated statement count times.
     */
    bool processTIMESDirective(const TIMESDirective* directive);

    SemanticAnalyzer m_semantic_analyzer;  ///< Resolves symbols before code generation
    InstructionEncoder m_encoder;          ///< Handles 8086 instruction encoding
    std::vector<uint8_t> m_binary;         ///< Accumulated machine code output
    std::vector<AssembledLine> m_listing;  ///< Source-to-binary mapping
    ErrorReporter m_error_reporter;        ///< Collects code generation errors
    size_t m_current_address;              ///< Current position in output
};

} // namespace e2asm
