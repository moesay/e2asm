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

/** This is the output of the expression parser, defined here to avoid any
    circular dependancies
  */
struct AddressExpression {
    std::vector<std::string> registers;
    int64_t displacement = 0;
    bool has_displacement = false;
    std::string label_name;           // Label reference (if any)
    bool has_label = false;           // True if this references a label
};

struct ASTNode {
    SourceLocation location;

    explicit ASTNode(SourceLocation loc) : location(loc) {}
    virtual ~ASTNode() = default;
};

// Top level program
struct Program : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;

    explicit Program(SourceLocation loc = SourceLocation()) : ASTNode(loc) {}
};

struct Instruction : ASTNode {
    std::string mnemonic;
    std::vector<std::unique_ptr<Operand>> operands;

    // These will be filled by the semantic analyzer
    size_t assigned_address = 0;
    size_t estimated_size = 0;

    Instruction(std::string mn, SourceLocation loc)
        : ASTNode(loc), mnemonic(std::move(mn)) {}
};

struct Label : ASTNode {
    std::string name;

    Label(std::string n, SourceLocation loc)
        : ASTNode(loc), name(std::move(n)) {}
};

// Data val for directives
struct DataValue {
    enum class Type {
        NUMBER,     // Numeric constant
        STRING,     // String literal
        CHARACTER   // Character literal
    } type;

    int64_t number_value;
    std::string string_value;

    DataValue(int64_t num) : type(Type::NUMBER), number_value(num) {}
    DataValue(std::string str, Type t = Type::STRING)
        : type(t), number_value(0), string_value(std::move(str)) {}
};

struct DataDirective : ASTNode {
    enum class Size {
        BYTE,   // DB - 1 byte
        WORD,   // DW - 2 bytes
        DWORD,  // DD - 4 bytes
        QWORD,  // DQ - 8 bytes
        TBYTE   // DT - 10 bytes
    } size;

    std::vector<DataValue> values;

    DataDirective(Size s, SourceLocation loc)
        : ASTNode(loc), size(s) {}
};

struct EQUDirective : ASTNode {
    std::string name;
    int64_t value;

    EQUDirective(std::string n, int64_t val, SourceLocation loc)
        : ASTNode(loc), name(std::move(n)), value(val) {}
};

struct ORGDirective : ASTNode {
    int64_t address;

    ORGDirective(int64_t addr, SourceLocation loc)
        : ASTNode(loc), address(addr) {}
};

struct SEGMENTDirective : ASTNode {
    std::string name;

    SEGMENTDirective(std::string n, SourceLocation loc)
        : ASTNode(loc), name(std::move(n)) {}
};

struct ENDSDirective : ASTNode {
    std::string name;

    ENDSDirective(std::string n, SourceLocation loc)
        : ASTNode(loc), name(std::move(n)) {}
};

struct RESDirective : ASTNode {
    enum class Size {
        BYTE,   // RESB - 1 byte
        WORD,   // RESW - 2 bytes
        DWORD,  // RESD - 4 bytes
        QWORD,  // RESQ - 8 bytes
        TBYTE   // REST - 10 bytes
    } size;

    int64_t count;  // Number of elements to reserve

    RESDirective(Size s, int64_t cnt, SourceLocation loc)
        : ASTNode(loc), size(s), count(cnt) {}
};

struct TIMESDirective : ASTNode {
    int64_t count;  // Repeat count (evaluated)
    std::string count_expr;  // Original expression (like the famous bootloader thing "512-($-$$)")
    std::unique_ptr<ASTNode> repeated_node;  // Node to repeat

    TIMESDirective(int64_t cnt, std::string expr, SourceLocation loc)
        : ASTNode(loc), count(cnt), count_expr(std::move(expr)) {}
};

struct Operand : ASTNode {
    enum class Type {
        REGISTER,
        IMMEDIATE,
        MEMORY,
        LABEL_REF
    } type;

    Operand(Type t, SourceLocation loc) : ASTNode(loc), type(t) {}
};

struct RegisterOperand : Operand {
    std::string name;
    uint8_t size;
    uint8_t code;
    bool is_segment;      // true for ES, CS, SS, DS

    RegisterOperand(std::string n, uint8_t sz, uint8_t c, bool seg, SourceLocation loc)
        : Operand(Type::REGISTER, loc), name(std::move(n)), size(sz), code(c), is_segment(seg) {}
};

struct ImmediateOperand : Operand {
    int64_t value;
    uint8_t size_hint;    // 8 or 16 bits (0 = auto-detect)
    std::string label_name;    // Label reference (if any)
    bool has_label;            // True if this references a label

    ImmediateOperand(int64_t val, SourceLocation loc, uint8_t hint = 0)
        : Operand(Type::IMMEDIATE, loc), value(val), size_hint(hint), has_label(false) {}

    ImmediateOperand(std::string label, SourceLocation loc, uint8_t hint = 0)
        : Operand(Type::IMMEDIATE, loc), value(0), size_hint(hint),
          label_name(std::move(label)), has_label(true) {}
};

struct MemoryOperand : Operand {
    std::optional<std::string> segment_override;
    std::string address_expr;
    std::unique_ptr<AddressExpression> parsed_address;
    bool is_direct_address;
    uint16_t direct_address_value;
    uint8_t size_hint;                             // 8 or 16 bits (0 = auto-detect)

    MemoryOperand(std::string addr, SourceLocation loc, uint8_t hint = 0)
        : Operand(Type::MEMORY, loc)
        , address_expr(std::move(addr))
        , is_direct_address(false)
        , direct_address_value(0)
        , size_hint(hint) {}
};

struct LabelRef : Operand {
    std::string label;
    enum class JumpType { SHORT, NEAR, FAR } jump_type;

    LabelRef(std::string lbl, SourceLocation loc, JumpType jt = JumpType::NEAR)
        : Operand(Type::LABEL_REF, loc), label(std::move(lbl)), jump_type(jt) {}
};

} // namespace e2asm
