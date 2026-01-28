/**
 * @file semantic_analyzer.h
 * @brief Semantic analysis and address resolution phase
 *
 * The semantic analyzer is the third compilation phase. It walks the AST,
 * builds the symbol table, assigns addresses to all statements, and validates
 * that symbols are properly defined. Performs multiple passes to handle forward
 * references and optimize instruction sizes.
 */

#pragma once

#include "symbol_table.h"
#include "../parser/ast.h"
#include "../core/error.h"
#include <vector>
#include <memory>

namespace e2asm {

/**
 * @brief Address assignment for a single statement
 *
 * Tracks which statement gets placed at which address and how much space
 * it occupies. Used to generate listings and resolve label references.
 */
struct AddressInfo {
    size_t statement_index;  ///< Index in Program's statements vector
    uint64_t address;        ///< Memory address for this statement
    uint64_t size;           ///< Space consumed in bytes (may change between passes)
};

/**
 * @brief Performs semantic analysis and address assignment
 *
 * The semantic analyzer validates symbol usage and calculates final addresses
 * for all statements. It operates in multiple passes:
 *
 * **Pass 1: Symbol Discovery**
 * - Walks the AST and creates symbol table entries for all labels and EQU constants
 * - Assigns provisional addresses to each statement
 * - Labels get the address where they're defined
 * - Forward references create unresolved symbols
 *
 * **Pass 2+: Address Refinement**
 * - Resolves forward references now that all symbols are known
 * - Recalculates instruction sizes (some instructions have multiple encodings)
 * - Updates addresses if sizes changed
 * - Repeats until addresses stabilize
 *
 * **Future: Jump Optimization**
 * - Could shrink jumps from NEAR to SHORT when target is close enough
 * - Requires additional iteration as sizes change
 *
 * The analyzer also handles:
 * - Segment tracking for SEGMENT/ENDS directives
 * - ORG directive processing
 * - $ (current address) and $$ (section start) symbols
 * - Duplicate symbol detection
 */
class SemanticAnalyzer {
public:
    SemanticAnalyzer();

    /**
     * @brief Analyzes an AST and assigns addresses
     * @param program AST to analyze (must be valid syntax)
     * @return true if analysis succeeded, false if semantic errors occurred
     *
     * Modifies the Program's AST nodes by filling in address and size fields.
     * Check getErrors() for details if this returns false.
     */
    bool analyze(Program* program);

    /**
     * @brief Gets the symbol table with all resolved symbols
     * @return Symbol table (const reference)
     */
    const SymbolTable& getSymbolTable() const { return m_symbol_table; }

    /**
     * @brief Gets the symbol table for modification
     * @return Symbol table (mutable reference)
     */
    SymbolTable& getSymbolTable() { return m_symbol_table; }

    /**
     * @brief Gets the address assigned to a statement
     * @param statement_index Index in Program's statements vector
     * @return Address if assigned, nullopt if index is invalid
     */
    std::optional<uint64_t> getAddress(size_t statement_index) const;

    /**
     * @brief Gets the base address set by ORG directive
     * @return Origin address (default 0 if no ORG)
     */
    uint64_t getOriginAddress() const { return m_origin_address; }

    /**
     * @brief Gets all semantic errors encountered
     * @return Vector of errors (undefined symbols, duplicate definitions, etc.)
     */
    const std::vector<Error>& getErrors() const { return m_errors; }

    /**
     * @brief Resets analyzer to initial state
     *
     * Call between assembly runs to reuse the analyzer instance.
     */
    void clear();

private:
    /**
     * @brief First pass: discovers symbols and assigns initial addresses
     * @param program AST to process
     * @return true if successful
     *
     * Creates symbol table entries for labels and constants. Assigns
     * provisional addresses assuming worst-case instruction sizes.
     */
    bool pass1_buildSymbols(Program* program);

    /**
     * @brief Second pass: resolves symbols and refines addresses
     * @param program AST to process
     * @return true if addresses changed (need another iteration)
     *
     * Looks up forward references now that all symbols exist. Recalculates
     * instruction sizes with known operand values. Updates addresses if
     * sizes changed.
     */
    bool pass2_resolveSymbols(Program* program);

    /**
     * @brief Estimates instruction size in bytes
     * @param instr Instruction to measure
     * @return Size in bytes (1-6 for 8086)
     *
     * Size depends on operand types and whether addresses fit in smaller encodings.
     */
    uint64_t calculateInstructionSize(Instruction* instr);

    /**
     * @brief Calculates the ModRM + displacement size for a memory operand
     * @param mem Memory operand to measure
     * @return Size in bytes (1-3: 1 for ModRM, 0-2 for displacement)
     *
     * Accounts for register indirect (no disp), disp8, and disp16 cases.
     */
    uint64_t calculateMemoryEncodingSize(const MemoryOperand* mem);

    /**
     * @brief Calculates size of data directive output
     * @param directive Directive name (DB, DW, etc.)
     * @param value_count Number of values
     * @return Total size in bytes
     */
    uint64_t calculateDataSize(const std::string& directive, size_t value_count);

    /**
     * @brief Reports a semantic error
     * @param message Error description
     * @param loc Source location where error occurred
     */
    void error(const std::string& message, SourceLocation loc);

    /**
     * @brief Information about an active segment
     */
    struct SegmentInfo {
        std::string name;          ///< Segment name
        uint64_t start_address;    ///< Address where segment began
        uint64_t current_address;  ///< Current position in segment
    };

    /**
     * @brief Sets the origin address
     * @param address Base address from ORG directive
     */
    void setOrigin(uint64_t address);

    /**
     * @brief Enters a new segment
     * @param name Segment name from SEGMENT directive
     */
    void enterSegment(const std::string& name);

    /**
     * @brief Exits current segment
     * @param name Segment name from ENDS directive (must match)
     */
    void exitSegment(const std::string& name);

    SymbolTable m_symbol_table;          ///< All labels and constants
    std::vector<AddressInfo> m_addresses; ///< Address assignment for each statement
    std::vector<Error> m_errors;         ///< Accumulated semantic errors
    uint64_t m_current_address;          ///< Next available address ($ symbol)

    std::vector<SegmentInfo> m_segments; ///< Stack of nested segments
    std::string m_current_segment;       ///< Name of active segment
    uint64_t m_segment_start_address;    ///< Start of current segment ($$ symbol)
    uint64_t m_origin_address;           ///< Base address from ORG directive
    bool m_last_was_terminator;          ///< Prevents fall-through between segments

    /**
     * @brief Checks if segment is a code segment
     * @param name Segment name
     * @return true for typical code segment names
     */
    bool isCodeSegment(const std::string& name) const;

    /**
     * @brief Checks if segment is a data segment
     * @param name Segment name
     * @return true for typical data segment names
     */
    bool isDataSegment(const std::string& name) const;

    /**
     * @brief Resolves a symbol name to its numeric value
     * @param name Symbol name to look up
     * @param loc Source location for error reporting
     * @param out_value Output parameter for resolved value
     * @return true if symbol was found and resolved, false otherwise
     */
    bool resolveSymbol(const std::string& name, SourceLocation loc, int64_t& out_value);

    /**
     * @brief Resolves all SYMBOL type values in a DataDirective to numbers
     * @param data Data directive to process
     * @return true if all symbols resolved successfully
     */
    bool resolveDataSymbols(DataDirective* data);

    /**
     * @brief Resolves memory operand expressions in an instruction
     * @param instr Instruction to process
     * @return true if all memory operand expressions resolved successfully
     *
     * Re-parses memory address expressions with symbol lookup to resolve
     * EQU constants in expressions like [label + s_size * 2 - 2].
     */
    bool resolveMemoryOperands(Instruction* instr);

    /**
     * @brief Creates a symbol lookup callback for expression evaluation
     * @return Callback function that looks up symbols in the symbol table
     */
    std::function<std::optional<int64_t>(const std::string&)> createSymbolLookup();
};

} // namespace e2asm
