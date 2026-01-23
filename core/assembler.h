/**
 * @file assembler.h
 * @brief Main interface to the E2Asm assembler
 *
 * This file contains the primary API for embedding the assembler into other projects.
 * The Assembler class orchestrates the entire assembly process from source text to
 * machine code, handling all phases transparently.
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include "error.h"

namespace e2asm {

/**
 * @brief Represents a single line of assembled code with its metadata
 *
 * Each line tracks the original source, generated machine code, and its
 * final address in the binary. Used for generating detailed assembly listings
 * that show the correspondence between source and output.
 */
struct AssembledLine {
    size_t source_line;                   ///< Line number in the original source file
    std::string source_text;              ///< Original assembly text before processing
    std::vector<uint8_t> machine_code;    ///< Generated 8086 machine code bytes
    size_t address;                       ///< Memory address where this instruction is placed
    bool success;                         ///< Whether this line assembled without errors
    std::string error_message;            ///< Error description if assembly failed

    AssembledLine()
        : source_line(0), address(0), success(false) {}
};

/**
 * @brief Complete result of an assembly operation
 *
 * Contains everything produced by the assembler: the final binary, a detailed
 * listing showing each line's encoding, resolved symbol addresses, and any
 * errors or warnings encountered during assembly.
 */
struct AssemblyResult {
    std::vector<uint8_t> binary;          ///< Final 8086 machine code ready for execution
    std::vector<AssembledLine> listing;   ///< Detailed line-by-line assembly output
    std::map<std::string, size_t> symbols; ///< Resolved symbols (labels -> addresses)
    std::vector<Error> errors;            ///< All errors and warnings from assembly
    bool success;                         ///< True only if assembly completed without errors
    uint64_t origin_address;              ///< Base address specified by ORG directive (default: 0)

    AssemblyResult() : success(false), origin_address(0) {}

    /**
     * @brief Formats the assembly listing as human-readable text
     * @return Multi-line string showing addresses, machine code, and source for each line
     */
    std::string getListingText() const;

    /**
     * @brief Writes the assembled binary to a file
     * @param filename Path to the output file (typically .bin or .com)
     * @return true if file was written successfully, false on I/O error
     */
    bool writeBinary(const std::string& filename) const;
};

/**
 * @brief Main entry point for the E2Asm assembler
 *
 * This class provides a clean API for assembling 8086 code from strings or files.
 * It manages all internal state and compilation phases (preprocessing, lexing, parsing,
 * semantic analysis, and code generation) automatically.
 *
 * The assembler is designed to be embedded into larger systems like IDEs, emulators,
 * or educational tools. Multiple Assembler instances can coexist independently.
 *
 * @code
 * e2asm::Assembler asm;
 * asm.setOrigin(0x7C00);  // Boot sector address
 * auto result = asm.assemble("MOV AX, 0x13\nINT 0x10");
 * if (result.success) {
 *     result.writeBinary("output.bin");
 * }
 * @endcode
 */
class Assembler {
public:
    /**
     * @brief Constructs a new assembler with default settings
     *
     * The assembler starts with origin 0, no include paths, and warnings enabled.
     */
    Assembler();

    ~Assembler();

    /**
     * @brief Assembles 8086 assembly source code from a string
     *
     * Runs the full assembly pipeline: preprocessing, lexical analysis, parsing,
     * semantic analysis, and code generation. Performs multiple passes to resolve
     * forward references and calculate optimal instruction sizes.
     *
     * @param source The complete assembly source code as a string
     * @param filename Filename to display in error messages (doesn't need to be real)
     * @return AssemblyResult containing binary, listing, symbols, and any errors
     */
    AssemblyResult assemble(const std::string& source,
                           const std::string& filename = "<input>");

    /**
     * @brief Assembles 8086 code from a file on disk
     *
     * Convenience method that reads the file and calls assemble(). The filename
     * is automatically used for error reporting.
     *
     * @param filepath Path to the assembly source file
     * @return AssemblyResult containing binary, listing, symbols, and any errors
     */
    AssemblyResult assembleFile(const std::string& filepath);

    /**
     * @brief Sets the base memory address for the assembled code
     *
     * Equivalent to placing "ORG address" at the start of your code. Affects
     * label resolution and relative jumps. Common values: 0x0 (default), 0x100
     * (COM files), 0x7C00 (boot sector).
     *
     * @param origin Base address where code will be loaded in memory
     */
    void setOrigin(size_t origin);

    /**
     * @brief Configures search paths for %include directives
     *
     * When the preprocessor encounters %include "file.asm", it searches these
     * directories in order. The current directory is always searched first.
     *
     * @param paths Vector of directory paths to search for included files
     */
    void setIncludePaths(const std::vector<std::string>& paths);

    /**
     * @brief Controls whether warnings are reported
     *
     * When enabled, the assembler reports potential issues that don't prevent
     * assembly (e.g., unused labels, suspicious operand sizes). Warnings appear
     * in the errors vector but don't set success=false.
     *
     * @param enable true to report warnings, false to suppress them
     */
    void enableWarnings(bool enable);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;  ///< PIMPL pattern hides implementation details
};

} // namespace e2asm
