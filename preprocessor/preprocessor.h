#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "../core/error.h"

namespace e2asm {

// Macro definition
struct MacroDefinition {
    std::string name;
    std::vector<std::string> parameters;  // Parameter names (%1, %2, etc. or named params)
    std::vector<std::string> body;        // Lines of macro body
    size_t line_defined;                   // Source line where defined
};

// Preprocessor processes directives before assembly
class Preprocessor {
public:
    Preprocessor();

    // Process source code, expanding macros and conditionals
    // Returns preprocessed source and any errors encountered
    struct PreprocessResult {
        std::string source;              // Preprocessed source code
        std::vector<Error> errors;       // Errors encountered
        bool success;                    // True if no errors
    };

    PreprocessResult process(const std::string& source, const std::string& filename = "<input>");

    // Set include search paths
    void setIncludePaths(const std::vector<std::string>& paths);

    // Clear all definitions (for new assembly)
    void reset();

private:
    // Directive handlers
    void handleDefine(const std::string& line, size_t line_num);
    void handleUndef(const std::string& line, size_t line_num);
    void handleIfdef(const std::string& line, size_t line_num);
    void handleIfndef(const std::string& line, size_t line_num);
    void handleIf(const std::string& line, size_t line_num);
    void handleElif(const std::string& line, size_t line_num);
    void handleElse(size_t line_num);
    void handleEndif(size_t line_num);
    void handleMacro(const std::string& line, size_t line_num);
    void handleEndmacro(size_t line_num);
    void handleInclude(const std::string& line, size_t line_num);

    // Helper functions
    bool isDirective(const std::string& line) const;
    std::string getDirectiveName(const std::string& line) const;
    std::string expandDefines(const std::string& line);
    std::string expandMacro(const std::string& name, const std::vector<std::string>& args);
    bool evaluateExpression(const std::string& expr);
    std::vector<std::string> parseMacroArgs(const std::string& args_str);
    std::string trim(const std::string& str) const;
    std::string readFile(const std::string& filename);
    std::string findIncludeFile(const std::string& filename);

    // State
    std::unordered_map<std::string, std::string> m_defines;           // %define constants
    std::unordered_map<std::string, MacroDefinition> m_macros;        // %macro definitions
    std::vector<std::string> m_include_paths;                         // Include search paths
    std::vector<Error> m_errors;                                      // Accumulated errors
    std::string m_current_filename;                                   // Current file being processed

    // Conditional compilation state
    struct ConditionalBlock {
        bool is_true;           // Is this block active?
        bool has_true_branch;   // Has any branch been taken?
        size_t line_num;        // Line where block started
    };
    std::vector<ConditionalBlock> m_conditional_stack;

    // Macro recording state
    bool m_recording_macro = false;
    MacroDefinition m_current_macro;

    // Output buffer
    std::vector<std::string> m_output_lines;
};

} // namespace e2asm
