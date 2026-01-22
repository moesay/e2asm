#pragma once

#include <string>
#include <cstddef>

namespace e2asm {

// Mainly used to represent a source location for error reporting
struct SourceLocation {
    std::string filename;
    size_t line;          // 1-based line number
    size_t column;        // 1-based column number

    SourceLocation() : filename("<input>"), line(1), column(1) {}

    SourceLocation(std::string file, size_t ln, size_t col)
        : filename(std::move(file)), line(ln), column(col) {}

    std::string format() const {
        return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
};

} // namespace e2asm
