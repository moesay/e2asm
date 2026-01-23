/**
 * @file preprocessor.h
 * @brief Text preprocessing phase before assembly
 *
 * The preprocessor runs before the lexer, handling text-level transformations:
 * macros, conditional compilation, file inclusion, and constant substitution.
 * Similar to C preprocessor but adapted for assembly syntax.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "../core/error.h"

namespace e2asm {

/**
 * @brief A user-defined macro with parameters
 *
 * Macros allow code templates that are expanded at preprocessing time.
 * Example:
 * @code
 * %macro PUSH_ALL 0
 *   push ax
 *   push bx
 *   push cx
 * %endmacro
 *
 * PUSH_ALL  ; Expands to the three push instructions
 * @endcode
 */
struct MacroDefinition {
    std::string name;                   ///< Macro identifier
    std::vector<std::string> parameters; ///< Parameter names (%1, %2, etc.)
    std::vector<std::string> body;       ///< Lines of macro body
    size_t line_defined;                 ///< Source line where macro was defined
};

/**
 * @brief Text preprocessor for assembly source
 *
 * The preprocessor handles directives that operate on source text before
 * actual assembly begins. It processes:
 *
 * **Constant Substitution** (%define)
 * - `%define WIDTH 80` - Define text replacement
 * - Occurrences of WIDTH in source are replaced with 80
 *
 * **Macros** (%macro/%endmacro)
 * - Parameterized code templates
 * - Parameters referenced as %1, %2, etc.
 * - Expanded inline at call site
 *
 * **Conditional Compilation** (%if/%ifdef/%ifndef/%elif/%else/%endif)
 * - Include/exclude code based on conditions
 * - `%ifdef DEBUG` includes code only if DEBUG is defined
 * - Allows building different configurations from same source
 *
 * **File Inclusion** (%include)
 * - Insert another file's contents at that point
 * - Searches configured include paths
 * - Useful for sharing constants and macros
 *
 * The preprocessor outputs plain assembly source with all directives resolved.
 * This processed source then goes to the lexer.
 */
class Preprocessor {
public:
    Preprocessor();

    /**
     * @brief Result of preprocessing operation
     */
    struct PreprocessResult {
        std::string source;        ///< Processed source with expansions applied
        std::vector<Error> errors; ///< Any preprocessing errors
        bool success;              ///< True if preprocessing succeeded
    };

    /**
     * @brief Preprocesses assembly source code
     * @param source Raw assembly source with preprocessor directives
     * @param filename Filename for error reporting
     * @return Processed source or errors
     *
     * Processes all preprocessor directives in order: includes, defines,
     * macros, conditionals. The output source contains no preprocessor
     * directives - they're all resolved to plain assembly.
     */
    PreprocessResult process(const std::string& source, const std::string& filename = "<input>");

    /**
     * @brief Configures directories to search for %include files
     * @param paths Vector of directory paths
     *
     * When %include "file.asm" is encountered, these directories are
     * searched in order. Current directory is always searched first.
     */
    void setIncludePaths(const std::vector<std::string>& paths);

    /**
     * @brief Clears all definitions and state
     *
     * Call between assembly runs to reuse the same preprocessor instance.
     * Removes all %define constants and %macro definitions.
     */
    void reset();

private:
    /** @brief Handles %define name value directive */
    void handleDefine(const std::string& line, size_t line_num);

    /** @brief Handles %undef name directive */
    void handleUndef(const std::string& line, size_t line_num);

    /** @brief Handles %ifdef name conditional */
    void handleIfdef(const std::string& line, size_t line_num);

    /** @brief Handles %ifndef name conditional */
    void handleIfndef(const std::string& line, size_t line_num);

    /** @brief Handles %if expression conditional */
    void handleIf(const std::string& line, size_t line_num);

    /** @brief Handles %elif expression (else-if) */
    void handleElif(const std::string& line, size_t line_num);

    /** @brief Handles %else directive */
    void handleElse(size_t line_num);

    /** @brief Handles %endif directive */
    void handleEndif(size_t line_num);

    /** @brief Handles %macro name param_count directive */
    void handleMacro(const std::string& line, size_t line_num);

    /** @brief Handles %endmacro directive */
    void handleEndmacro(size_t line_num);

    /** @brief Handles %include "filename" directive */
    void handleInclude(const std::string& line, size_t line_num);

    /** @brief Checks if line starts with % (preprocessor directive) */
    bool isDirective(const std::string& line) const;

    /** @brief Extracts directive name from line */
    std::string getDirectiveName(const std::string& line) const;

    /** @brief Replaces %define constants in line */
    std::string expandDefines(const std::string& line);

    /** @brief Expands a macro invocation with arguments */
    std::string expandMacro(const std::string& name, const std::vector<std::string>& args);

    /** @brief Evaluates constant expression for %if */
    bool evaluateExpression(const std::string& expr);

    /** @brief Parses comma-separated macro arguments */
    std::vector<std::string> parseMacroArgs(const std::string& args_str);

    /** @brief Removes leading/trailing whitespace */
    std::string trim(const std::string& str) const;

    /** @brief Reads file contents as string */
    std::string readFile(const std::string& filename);

    /** @brief Searches include paths for file */
    std::string findIncludeFile(const std::string& filename);

    std::unordered_map<std::string, std::string> m_defines;      ///< %define constants
    std::unordered_map<std::string, MacroDefinition> m_macros;   ///< %macro definitions
    std::vector<std::string> m_include_paths;                    ///< Directories to search
    std::vector<Error> m_errors;                                 ///< Accumulated errors
    std::string m_current_filename;                              ///< Current file being processed

    /**
     * @brief State for nested conditional blocks
     */
    struct ConditionalBlock {
        bool is_true;           ///< Is this block's condition satisfied?
        bool has_true_branch;   ///< Has any branch in this if/elif chain been taken?
        size_t line_num;        ///< Line where block started (for error reporting)
    };
    std::vector<ConditionalBlock> m_conditional_stack;  ///< Stack of nested %if blocks

    bool m_recording_macro = false;      ///< Currently inside %macro/%endmacro
    MacroDefinition m_current_macro;     ///< Macro being recorded

    std::vector<std::string> m_output_lines;  ///< Accumulated preprocessed output
};

} // namespace e2asm
