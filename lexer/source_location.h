/**
 * @file source_location.h
 * @brief Source code position tracking for error reporting
 *
 * Every token and AST node carries a SourceLocation so errors can point to
 * the exact file, line, and column where a problem occurred.
 */

#pragma once

#include <string>
#include <cstddef>

namespace e2asm {

/**
 * @brief Pinpoints an exact position in source code
 *
 * Tracks filename, line, and column for every element in the compilation pipeline.
 * Line and column numbers are 1-based to match how text editors display positions.
 * This enables precise error messages like "boot.asm:42:10: error: undefined label".
 */
struct SourceLocation {
    std::string filename;  ///< Source file path, or "<input>" for string sources
    size_t line;           ///< 1-based line number (first line is 1)
    size_t column;         ///< 1-based column number (first character is 1)

    /**
     * @brief Creates a default location for anonymous input
     */
    SourceLocation() : filename("<input>"), line(1), column(1) {}

    /**
     * @brief Creates a location for a specific position
     * @param file Source filename or path
     * @param ln Line number (1-based)
     * @param col Column number (1-based)
     */
    SourceLocation(std::string file, size_t ln, size_t col)
        : filename(std::move(file)), line(ln), column(col) {}

    /**
     * @brief Formats location in compiler-standard format
     * @return String like "file.asm:10:5" compatible with IDE error parsers
     */
    std::string format() const {
        return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
};

} // namespace e2asm
