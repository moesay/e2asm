#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include "error.h"

namespace e2asm {

struct AssembledLine {
    size_t source_line;
    std::string source_text;      // Original assembly text
    std::vector<uint8_t> machine_code;
    size_t address;
    bool success;
    std::string error_message;

    AssembledLine()
        : source_line(0), address(0), success(false) {}
};

struct AssemblyResult {
    std::vector<uint8_t> binary;
    std::vector<AssembledLine> listing;
    std::map<std::string, size_t> symbols; // Symbol table (label -> address)
    std::vector<Error> errors;
    bool success;
    uint64_t origin_address;              // defaut is zero

    AssemblyResult() : success(false), origin_address(0) {}

    std::string getListingText() const;

    bool writeBinary(const std::string& filename) const;
};

class Assembler {
public:
    Assembler();
    ~Assembler();

    // filename here is mainly for error reporting
    AssemblyResult assemble(const std::string& source,
                           const std::string& filename = "<input>");

    AssemblyResult assembleFile(const std::string& filepath);

    void setOrigin(size_t origin);

    void setIncludePaths(const std::vector<std::string>& paths);

    void enableWarnings(bool enable);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace e2asm
