#include "assembler.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../codegen/code_generator.h"
#include "../preprocessor/preprocessor.h"
#include <fstream>
#include <sstream>

namespace e2asm {

/**
  Why pimpl?
  Because I want to lol.
  1- who knows how many versions i will release, pimpl will provide ABI stability across releases.
  2- Implementation hiding so users can consume the lib without getting bothered with source files.

  Overall, cleaner public API...and also, because I want to.
*/
class Assembler::Impl {
public:
    size_t origin = 0;
    std::vector<std::string> include_paths;
    bool warnings_enabled = true;

    AssemblyResult assemble(const std::string& source, const std::string& filename) {
        AssemblyResult result;

        // Phase 0: Preprocessing
        Preprocessor preprocessor;
        preprocessor.setIncludePaths(include_paths);
        auto preprocess_result = preprocessor.process(source, filename);

        if (!preprocess_result.success) {
            result.errors = preprocess_result.errors;
            result.success = false;
            return result;
        }

        // Phase 1: Lexical analysis
        Lexer lexer(preprocess_result.source, filename);
        std::vector<Token> tokens = lexer.tokenize();

        // Phase 2: Parsing
        Parser parser(std::move(tokens));
        auto ast = parser.parse();

        if (parser.hasErrors()) {
            result.errors = parser.errors();
            result.success = false;
            return result;
        }

        // For now, the semantic analysis is a part of the code generation process
        // TODO: Phase 3: Semantic Analysis
        // Maybe one day i will seperate them, maybe not.

        // Phase 4: Code generation
        CodeGenerator generator;
        result = generator.generate(ast.get());

        return result;
    }
};

Assembler::Assembler()
    : m_impl(std::make_unique<Impl>())
{
}

Assembler::~Assembler() = default;

AssemblyResult Assembler::assemble(const std::string& source, const std::string& filename) {
    return m_impl->assemble(source, filename);
}

AssemblyResult Assembler::assembleFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        AssemblyResult result;
        result.success = false;
        result.errors.push_back(Error("Could not open file: " + filepath,
                                     SourceLocation(filepath, 0, 0)));
        return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return assemble(buffer.str(), filepath);
}

void Assembler::setOrigin(size_t origin) {
    m_impl->origin = origin;
}

void Assembler::setIncludePaths(const std::vector<std::string>& paths) {
    m_impl->include_paths = paths;
}

void Assembler::enableWarnings(bool enable) {
    m_impl->warnings_enabled = enable;
}

std::string AssemblyResult::getListingText() const {
    std::string listing;
    for (const auto& line : this->listing) {
        // Format: address | machine code | source
        char addr_buf[16];
        snprintf(addr_buf, sizeof(addr_buf), "%04zX", line.address);
        listing += addr_buf;
        listing += " | ";

        // Machine code bytes
        for (uint8_t byte : line.machine_code) {
            char byte_buf[4];
            snprintf(byte_buf, sizeof(byte_buf), "%02X ", byte);
            listing += byte_buf;
        }

        listing += " | ";
        listing += line.source_text;
        listing += "\n";
    }
    return listing;
}

bool AssemblyResult::writeBinary(const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(binary.data()), binary.size());
    return file.good();
}

} // namespace e2asm
