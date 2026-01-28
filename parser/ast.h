/**
 * @file ast.h
 * @brief Abstract Syntax Tree node definitions
 *
 * The AST represents the parsed structure of assembly source code in a form that's
 * easy to analyze and generate code from. Each construct (instruction, directive,
 * label) becomes a specific node type in the tree.
 */

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <cstdint>
#include "../lexer/source_location.h"

namespace e2asm {

struct ASTNode;
struct Program;
struct Instruction;
struct Label;
struct DataDirective;
struct EQUDirective;
struct ORGDirective;
struct SEGMENTDirective;
struct ENDSDirective;
struct RESDirective;
struct TIMESDirective;
struct Operand;
struct RegisterOperand;
struct ImmediateOperand;
struct MemoryOperand;
struct LabelRef;

/**
 * @brief Parsed memory address expression like [BX+SI+10] or [label+4]
 *
 * The expression parser breaks down complex address calculations into components
 * that the code generator can encode as ModR/M bytes. Handles 8086 addressing
 * modes with base+index registers and displacements.
 */
struct AddressExpression {
    std::vector<std::string> registers; ///< Base/index regs (e.g., "BX", "SI")
    int64_t displacement = 0;           ///< Numeric offset added to address
    bool has_displacement = false;      ///< Whether displacement is present
    std::string label_name;             ///< Symbol reference in address (e.g., [label+BX])
    bool has_label = false;             ///< Whether a label is referenced
};

/**
 * @brief Base class for all AST nodes
 *
 * Every element in the syntax tree inherits from ASTNode and carries its
 * source location. Enables polymorphic traversal of the tree and precise
 * error reporting.
 */
struct ASTNode {
    SourceLocation location;  ///< Where in source this construct appeared

    explicit ASTNode(SourceLocation loc) : location(loc) {}
    virtual ~ASTNode() = default;
};

/**
 * @brief Root of the AST representing a complete assembly file
 *
 * Contains all top-level statements (instructions, labels, directives) in
 * the order they appear in source. The semantic analyzer and code generator
 * process these statements sequentially.
 */
struct Program : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;  ///< All top-level constructs

    explicit Program(SourceLocation loc = SourceLocation()) : ASTNode(loc) {}
};

/**
 * @brief Represents a machine instruction like MOV AX, BX
 *
 * Instructions are the core executable statements. Each has a mnemonic (operation
 * name) and 0-2 operands. The semantic analyzer fills in address and size info,
 * then the code generator emits the actual machine bytes.
 */
struct Instruction : ASTNode {
    std::string mnemonic;                        ///< Operation name (MOV, ADD, JMP, etc.)
    std::vector<std::unique_ptr<Operand>> operands;  ///< Destination and source operands

    size_t assigned_address = 0;  ///< Memory address assigned by semantic analyzer
    size_t estimated_size = 0;    ///< Instruction size in bytes (1-6 for 8086)

    Instruction(std::string mn, SourceLocation loc)
        : ASTNode(loc), mnemonic(std::move(mn)) {}
};

/**
 * @brief Represents a symbolic address marker
 *
 * Labels mark positions in code or data that can be referenced elsewhere.
 * They're resolved to addresses during semantic analysis. Support both global
 * labels (start, loop) and local labels (.retry, .done).
 */
struct Label : ASTNode {
    std::string name;  ///< Label identifier (e.g., "start" or ".loop")

    Label(std::string n, SourceLocation loc)
        : ASTNode(loc), name(std::move(n)) {}
};

/**
 * @brief A single value in a data directive
 *
 * DB/DW/DD directives can contain numbers, strings, or characters.
 * This variant type holds any of those forms.
 */
struct DataValue {
    enum class Type {
        NUMBER,     ///< Numeric constant (0x42, 65, etc.)
        STRING,     ///< String literal ("hello")
        CHARACTER,  ///< Character literal ('A')
        SYMBOL      ///< Unresolved symbol (EQU constant or label)
    } type;

    int64_t number_value;      ///< Value when type is NUMBER (or resolved SYMBOL)
    std::string string_value;  ///< Content when type is STRING, CHARACTER, or SYMBOL name

    DataValue(int64_t num) : type(Type::NUMBER), number_value(num) {}
    DataValue(std::string str, Type t = Type::STRING)
        : type(t), number_value(0), string_value(std::move(str)) {}
};

/**
 * @brief Data definition directive (DB, DW, DD, DQ, DT)
 *
 * Defines initialized data in the output. Can mix numbers, strings, and
 * characters: "DB 'Hello', 0, 13, 10" becomes bytes in the binary.
 */
struct DataDirective : ASTNode {
    enum class Size {
        BYTE,   ///< DB - 1 byte per value
        WORD,   ///< DW - 2 bytes per value
        DWORD,  ///< DD - 4 bytes per value
        QWORD,  ///< DQ - 8 bytes per value
        TBYTE   ///< DT - 10 bytes per value (extended precision)
    } size;

    std::vector<DataValue> values;  ///< All values to emit

    DataDirective(Size s, SourceLocation loc)
        : ASTNode(loc), size(s) {}
};

/**
 * @brief Constant definition (name EQU value)
 *
 * Defines a symbolic constant that can be used in expressions. Unlike labels,
 * EQU constants don't consume space in the output - they're purely compile-time.
 * Example: "WIDTH EQU 80" allows using WIDTH in place of 80.
 */
struct EQUDirective : ASTNode {
    std::string name;  ///< Constant name
    int64_t value;     ///< Constant value (must be computable at assembly time)

    EQUDirective(std::string n, int64_t val, SourceLocation loc)
        : ASTNode(loc), name(std::move(n)), value(val) {}
};

/**
 * @brief Origin directive (ORG address)
 *
 * Sets the base address where code will be loaded. Affects all label addresses
 * and relative jumps. Common values: 0x100 (COM programs), 0x7C00 (boot sector).
 */
struct ORGDirective : ASTNode {
    int64_t address;  ///< Base load address

    ORGDirective(int64_t addr, SourceLocation loc)
        : ASTNode(loc), address(addr) {}
};

/**
 * @brief Segment start directive (SEGMENT name or SECTION name)
 *
 * Begins a named segment for organizing code and data. Helps structure
 * larger programs into logical sections. SECTION is a synonym for SEGMENT.
 */
struct SEGMENTDirective : ASTNode {
    std::string name;  ///< Segment name (e.g., "CODE", "DATA", ".text")

    SEGMENTDirective(std::string n, SourceLocation loc)
        : ASTNode(loc), name(std::move(n)) {}
};

/**
 * @brief Segment end directive (name ENDS)
 *
 * Closes a segment started by SEGMENT. The name must match the opening
 * SEGMENT directive.
 */
struct ENDSDirective : ASTNode {
    std::string name;  ///< Name of segment being closed

    ENDSDirective(std::string n, SourceLocation loc)
        : ASTNode(loc), name(std::move(n)) {}
};

/**
 * @brief Reserve space directive (RESB, RESW, RESD, RESQ, REST)
 *
 * Reserves uninitialized space in the output. Useful for BSS sections or
 * creating buffers. "RESB 100" reserves 100 bytes without specifying contents.
 */
struct RESDirective : ASTNode {
    enum class Size {
        BYTE,   ///< RESB - 1 byte per unit
        WORD,   ///< RESW - 2 bytes per unit
        DWORD,  ///< RESD - 4 bytes per unit
        QWORD,  ///< RESQ - 8 bytes per unit
        TBYTE   ///< REST - 10 bytes per unit
    } size;

    int64_t count;  ///< Number of units to reserve

    RESDirective(Size s, int64_t cnt, SourceLocation loc)
        : ASTNode(loc), size(s), count(cnt) {}
};

/**
 * @brief Repetition directive (TIMES count instruction/data)
 *
 * Repeats the following instruction or directive a specified number of times.
 * Famous use: "TIMES 510-($-$$) DB 0" pads a boot sector to 512 bytes.
 * Supports expressions with $ (current address) and $$ (section start).
 */
struct TIMESDirective : ASTNode {
    int64_t count;                          ///< Evaluated repetition count
    std::string count_expr;                 ///< Original expression (e.g., "512-($-$$)")
    std::unique_ptr<ASTNode> repeated_node; ///< What to repeat

    TIMESDirective(int64_t cnt, std::string expr, SourceLocation loc)
        : ASTNode(loc), count(cnt), count_expr(std::move(expr)) {}
};

/**
 * @brief Base class for instruction operands
 *
 * Operands are the arguments to instructions. The 8086 supports several
 * addressing modes: registers, immediates, memory addresses, and labels.
 * Each mode has its own subclass with specific properties.
 */
struct Operand : ASTNode {
    enum class Type {
        REGISTER,   ///< Register operand (AX, BL, ES, etc.)
        IMMEDIATE,  ///< Constant value or symbol
        MEMORY,     ///< Memory address expression [BX+SI+10]
        LABEL_REF   ///< Label for jumps/calls
    } type;

    Operand(Type t, SourceLocation loc) : ASTNode(loc), type(t) {}
};

/**
 * @brief Register operand (AX, BL, SI, ES, etc.)
 *
 * Represents direct use of a CPU register. Tracks the register's encoding
 * value, size, and whether it's a segment register.
 */
struct RegisterOperand : Operand {
    std::string name;     ///< Register name as written ("AX", "BL", etc.)
    uint8_t size;         ///< 8 or 16 bits
    uint8_t code;         ///< 3-bit encoding value (0-7) for ModR/M byte
    bool is_segment;      ///< true for ES, CS, SS, DS

    RegisterOperand(std::string n, uint8_t sz, uint8_t c, bool seg, SourceLocation loc)
        : Operand(Type::REGISTER, loc), name(std::move(n)), size(sz), code(c), is_segment(seg) {}
};

/**
 * @brief Immediate value operand (constant or symbolic)
 *
 * Represents a numeric constant embedded in the instruction (MOV AX, 42).
 * Can also reference labels or EQU constants, which are resolved during
 * semantic analysis. The size_hint helps the encoder choose byte vs word encoding.
 */
struct ImmediateOperand : Operand {
    int64_t value;              ///< Numeric value (if not a symbol)
    uint8_t size_hint;          ///< 8 or 16 bits, 0 means infer from context
    std::string label_name;     ///< Symbol being referenced
    bool has_label;             ///< true if this is a symbol, not a number

    ImmediateOperand(int64_t val, SourceLocation loc, uint8_t hint = 0)
        : Operand(Type::IMMEDIATE, loc), value(val), size_hint(hint), has_label(false) {}

    ImmediateOperand(std::string label, SourceLocation loc, uint8_t hint = 0)
        : Operand(Type::IMMEDIATE, loc), value(0), size_hint(hint),
          label_name(std::move(label)), has_label(true) {}
};

/**
 * @brief Memory address operand [...]
 *
 * Represents memory access through an address expression. Supports:
 * - Direct addresses: [1234]
 * - Base+index: [BX+SI]
 * - With displacement: [BX+10], [label+4]
 * - Segment override: ES:[BX]
 * - Size hints: BYTE [BX], WORD [SI]
 */
struct MemoryOperand : Operand {
    std::optional<std::string> segment_override;   ///< ES/CS/SS/DS if specified
    std::string address_expr;                      ///< Original bracketed expression
    std::unique_ptr<AddressExpression> parsed_address; ///< Parsed components
    bool is_direct_address;                        ///< true for [1234] form
    uint16_t direct_address_value;                 ///< Value when is_direct_address
    uint8_t size_hint;                             ///< 8 or 16 bits, 0 means infer

    MemoryOperand(std::string addr, SourceLocation loc, uint8_t hint = 0)
        : Operand(Type::MEMORY, loc)
        , address_expr(std::move(addr))
        , is_direct_address(false)
        , direct_address_value(0)
        , size_hint(hint) {}
};

/**
 * @brief Label reference for control flow (JMP, CALL)
 *
 * Jump and call instructions reference labels without brackets. The jump
 * type determines encoding: SHORT (8-bit offset), NEAR (16-bit within segment),
 * or FAR (segment:offset).
 */
struct LabelRef : Operand {
    std::string label;                               ///< Target label name
    enum class JumpType { SHORT, NEAR, FAR } jump_type; ///< Jump distance hint

    LabelRef(std::string lbl, SourceLocation loc, JumpType jt = JumpType::NEAR)
        : Operand(Type::LABEL_REF, loc), label(std::move(lbl)), jump_type(jt) {}
};

} // namespace e2asm
