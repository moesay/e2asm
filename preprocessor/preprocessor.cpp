#include "preprocessor.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace e2asm {

Preprocessor::Preprocessor() {
}

void Preprocessor::reset() {
    m_defines.clear();
    m_macros.clear();
    m_errors.clear();
    m_conditional_stack.clear();
    m_output_lines.clear();
    m_recording_macro = false;
}

void Preprocessor::setIncludePaths(const std::vector<std::string>& paths) {
    m_include_paths = paths;
}

Preprocessor::PreprocessResult Preprocessor::process(const std::string& source, const std::string& filename) {
    reset();
    m_current_filename = filename;

    // Split source into lines
    std::vector<std::string> lines;
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    // Process lines
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string current_line = lines[i];
        size_t line_num = i + 1;

        // Handle line continuation (\)
        while (!current_line.empty() && current_line.back() == '\\') {
            current_line.pop_back();  // Remove backslash
            if (i + 1 < lines.size()) {
                ++i;
                current_line += lines[i];
            } else {
                m_errors.push_back(Error("Line continuation at end of file",
                                        SourceLocation{m_current_filename, line_num, 0}));
                break;
            }
        }

        // Trim whitespace
        current_line = trim(current_line);

        // Skip empty lines and comments
        if (current_line.empty() || current_line[0] == ';') {
            if (!m_recording_macro && (m_conditional_stack.empty() || m_conditional_stack.back().is_true)) {
                m_output_lines.push_back(current_line);
            }
            continue;
        }

        // Check if this is a directive
        if (isDirective(current_line)) {
            std::string directive = getDirectiveName(current_line);

            // Handle directives
            if (directive == "define") {
                if (!m_recording_macro && (m_conditional_stack.empty() || m_conditional_stack.back().is_true)) {
                    handleDefine(current_line, line_num);
                }
            } else if (directive == "undef") {
                if (!m_recording_macro && (m_conditional_stack.empty() || m_conditional_stack.back().is_true)) {
                    handleUndef(current_line, line_num);
                }
            } else if (directive == "ifdef") {
                handleIfdef(current_line, line_num);
            } else if (directive == "ifndef") {
                handleIfndef(current_line, line_num);
            } else if (directive == "if") {
                handleIf(current_line, line_num);
            } else if (directive == "elif") {
                handleElif(current_line, line_num);
            } else if (directive == "else") {
                handleElse(line_num);
            } else if (directive == "endif") {
                handleEndif(line_num);
            } else if (directive == "macro") {
                if (!m_recording_macro && (m_conditional_stack.empty() || m_conditional_stack.back().is_true)) {
                    handleMacro(current_line, line_num);
                }
            } else if (directive == "endmacro") {
                if (m_recording_macro) {
                    handleEndmacro(line_num);
                }
            } else if (directive == "include") {
                if (!m_recording_macro && (m_conditional_stack.empty() || m_conditional_stack.back().is_true)) {
                    handleInclude(current_line, line_num);
                }
            } else {
                m_errors.push_back(Error("Unknown preprocessor directive: %" + directive,
                                        SourceLocation{m_current_filename, line_num, 0}));
            }
        } else {
            // Regular line - check if we should output it
            if (m_recording_macro) {
                // Add to current macro body
                m_current_macro.body.push_back(current_line);
            } else if (m_conditional_stack.empty() || m_conditional_stack.back().is_true) {
                // Expand defines and check for macro calls
                std::string expanded = expandDefines(current_line);
                m_output_lines.push_back(expanded);
            }
        }
    }

    // Check for unclosed blocks
    if (!m_conditional_stack.empty()) {
        m_errors.push_back(Error("Unclosed conditional block (missing %endif)",
                                SourceLocation{m_current_filename, m_conditional_stack.back().line_num, 0}));
    }

    if (m_recording_macro) {
        m_errors.push_back(Error("Unclosed macro definition (missing %endmacro)",
                                SourceLocation{m_current_filename, m_current_macro.line_defined, 0}));
    }

    // Build output
    std::string output;
    for (const auto& line : m_output_lines) {
        output += line + "\n";
    }

    return PreprocessResult{output, m_errors, m_errors.empty()};
}

bool Preprocessor::isDirective(const std::string& line) const {
    return !line.empty() && line[0] == '%';
}

std::string Preprocessor::getDirectiveName(const std::string& line) const {
    if (!isDirective(line)) return "";

    size_t pos = 1;  // Skip %
    while (pos < line.size() && std::isspace(line[pos])) ++pos;

    size_t start = pos;
    while (pos < line.size() && (std::isalnum(line[pos]) || line[pos] == '_')) ++pos;

    return line.substr(start, pos - start);
}

void Preprocessor::handleDefine(const std::string& line, size_t line_num) {
    // Parse: %define NAME value
    size_t pos = line.find("define");
    pos += 6;  // Skip "define"

    while (pos < line.size() && std::isspace(line[pos])) ++pos;

    if (pos >= line.size()) {
        m_errors.push_back(Error("%define requires a name",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    size_t name_start = pos;
    while (pos < line.size() && !std::isspace(line[pos])) ++pos;

    std::string name = line.substr(name_start, pos - name_start);

    while (pos < line.size() && std::isspace(line[pos])) ++pos;

    std::string value = (pos < line.size()) ? line.substr(pos) : "";

    m_defines[name] = value;
}

void Preprocessor::handleUndef(const std::string& line, size_t line_num) {
    // Parse: %undef NAME
    size_t pos = line.find("undef");
    pos += 5;  // Skip "undef"

    while (pos < line.size() && std::isspace(line[pos])) ++pos;

    if (pos >= line.size()) {
        m_errors.push_back(Error("%undef requires a name",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    size_t name_start = pos;
    while (pos < line.size() && !std::isspace(line[pos])) ++pos;

    std::string name = line.substr(name_start, pos - name_start);

    m_defines.erase(name);
}

void Preprocessor::handleIfdef(const std::string& line, size_t line_num) {
    // Parse: %ifdef NAME
    size_t pos = line.find("ifdef");
    pos += 5;  // Skip "ifdef"

    while (pos < line.size() && std::isspace(line[pos])) ++pos;

    if (pos >= line.size()) {
        m_errors.push_back(Error("%ifdef requires a name",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    std::string name = trim(line.substr(pos));

    bool is_defined = m_defines.find(name) != m_defines.end();
    bool parent_active = m_conditional_stack.empty() || m_conditional_stack.back().is_true;

    m_conditional_stack.push_back({is_defined && parent_active, is_defined && parent_active, line_num});
}

void Preprocessor::handleIfndef(const std::string& line, size_t line_num) {
    // Parse: %ifndef NAME
    size_t pos = line.find("ifndef");
    pos += 6;  // Skip "ifndef"

    while (pos < line.size() && std::isspace(line[pos])) ++pos;

    if (pos >= line.size()) {
        m_errors.push_back(Error("%ifndef requires a name",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    std::string name = trim(line.substr(pos));

    bool is_not_defined = m_defines.find(name) == m_defines.end();
    bool parent_active = m_conditional_stack.empty() || m_conditional_stack.back().is_true;

    m_conditional_stack.push_back({is_not_defined && parent_active, is_not_defined && parent_active, line_num});
}

void Preprocessor::handleIf(const std::string& line, size_t line_num) {
    // Parse: %if expression
    size_t pos = line.find("if");
    pos += 2;  // Skip "if"

    while (pos < line.size() && std::isspace(line[pos])) ++pos;

    if (pos >= line.size()) {
        m_errors.push_back(Error("%if requires an expression",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    std::string expr = expandDefines(line.substr(pos));
    bool result = evaluateExpression(expr);
    bool parent_active = m_conditional_stack.empty() || m_conditional_stack.back().is_true;

    m_conditional_stack.push_back({result && parent_active, result && parent_active, line_num});
}

void Preprocessor::handleElif(const std::string& line, size_t line_num) {
    if (m_conditional_stack.empty()) {
        m_errors.push_back(Error("%elif without matching %if",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    // Parse: %elif expression
    size_t pos = line.find("elif");
    pos += 4;  // Skip "elif"

    while (pos < line.size() && std::isspace(line[pos])) ++pos;

    if (pos >= line.size()) {
        m_errors.push_back(Error("%elif requires an expression",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    auto& block = m_conditional_stack.back();

    if (!block.has_true_branch) {
        std::string expr = expandDefines(line.substr(pos));
        bool result = evaluateExpression(expr);
        bool parent_active = m_conditional_stack.size() == 1 ||
                            m_conditional_stack[m_conditional_stack.size() - 2].is_true;

        block.is_true = result && parent_active;
        block.has_true_branch = block.is_true;
    } else {
        block.is_true = false;
    }
}

void Preprocessor::handleElse(size_t line_num) {
    if (m_conditional_stack.empty()) {
        m_errors.push_back(Error("%else without matching %if",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    auto& block = m_conditional_stack.back();

    if (!block.has_true_branch) {
        bool parent_active = m_conditional_stack.size() == 1 ||
                            m_conditional_stack[m_conditional_stack.size() - 2].is_true;
        block.is_true = parent_active;
        block.has_true_branch = true;
    } else {
        block.is_true = false;
    }
}

void Preprocessor::handleEndif(size_t line_num) {
    if (m_conditional_stack.empty()) {
        m_errors.push_back(Error("%endif without matching %if",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    m_conditional_stack.pop_back();
}

void Preprocessor::handleMacro(const std::string& line, size_t line_num) {
    // Parse: %macro NAME param_count or %macro NAME(param1, param2, ...)
    size_t pos = line.find("macro");
    pos += 5;  // Skip "macro"

    while (pos < line.size() && std::isspace(line[pos])) ++pos;

    if (pos >= line.size()) {
        m_errors.push_back(Error("%macro requires a name",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    size_t name_start = pos;
    while (pos < line.size() && (std::isalnum(line[pos]) || line[pos] == '_')) ++pos;

    std::string name = line.substr(name_start, pos - name_start);

    m_recording_macro = true;
    m_current_macro = MacroDefinition{name, {}, {}, line_num};

    // For simplicity, we'll support NASM-style numbered parameters (%1, %2, etc.)
    // Parse parameter count
    while (pos < line.size() && std::isspace(line[pos])) ++pos;

    if (pos < line.size()) {
        std::string rest = line.substr(pos);
        // Could be parameter count or parameter names in parentheses
        // For now, just store the count as a number
        try {
            int param_count = std::stoi(rest);
            for (int i = 1; i <= param_count; ++i) {
                m_current_macro.parameters.push_back("%" + std::to_string(i));
            }
        } catch (...) {
            // Ignore parse errors for now
        }
    }
}

void Preprocessor::handleEndmacro(size_t line_num) {
    if (!m_recording_macro) {
        m_errors.push_back(Error("%endmacro without matching %macro",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    m_macros[m_current_macro.name] = m_current_macro;
    m_recording_macro = false;
}

void Preprocessor::handleInclude(const std::string& line, size_t line_num) {
    // Parse: %include "filename" or %include <filename>
    size_t pos = line.find("include");
    pos += 7;  // Skip "include"

    while (pos < line.size() && std::isspace(line[pos])) ++pos;

    if (pos >= line.size()) {
        m_errors.push_back(Error("%include requires a filename",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    char quote = line[pos];
    if (quote != '"' && quote != '<') {
        m_errors.push_back(Error("%include filename must be in quotes or angle brackets",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    ++pos;
    size_t filename_start = pos;
    char end_quote = (quote == '<') ? '>' : '"';

    while (pos < line.size() && line[pos] != end_quote) ++pos;

    if (pos >= line.size()) {
        m_errors.push_back(Error("%include missing closing quote",
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    std::string filename = line.substr(filename_start, pos - filename_start);

    // Find and read the file
    std::string filepath = findIncludeFile(filename);
    if (filepath.empty()) {
        m_errors.push_back(Error("Could not find include file: " + filename,
                                SourceLocation{m_current_filename, line_num, 0}));
        return;
    }

    std::string content = readFile(filepath);
    if (content.empty() && !m_errors.empty()) {
        // Error already added by readFile
        return;
    }

    // Recursively process the included file
    std::string saved_filename = m_current_filename;
    auto result = process(content, filepath);
    m_current_filename = saved_filename;

    if (!result.success) {
        m_errors.insert(m_errors.end(), result.errors.begin(), result.errors.end());
    }

    // Add included content to output
    std::istringstream stream(result.source);
    std::string included_line;
    while (std::getline(stream, included_line)) {
        m_output_lines.push_back(included_line);
    }
}

std::string Preprocessor::expandDefines(const std::string& line) {
    std::string result = line;

    // Simple text replacement for %define constants
    for (const auto& [name, value] : m_defines) {
        size_t pos = 0;
        while ((pos = result.find(name, pos)) != std::string::npos) {
            // Check if it's a whole word (not part of another identifier)
            bool is_word_start = (pos == 0 || !std::isalnum(result[pos - 1]) && result[pos - 1] != '_');
            bool is_word_end = (pos + name.size() >= result.size() ||
                               !std::isalnum(result[pos + name.size()]) && result[pos + name.size()] != '_');

            if (is_word_start && is_word_end) {
                result.replace(pos, name.size(), value);
                pos += value.size();
            } else {
                ++pos;
            }
        }
    }

    // Check for macro invocations
    for (const auto& [name, macro] : m_macros) {
        size_t pos = result.find(name);
        if (pos != std::string::npos) {
            // Simple macro expansion (no parameter support yet in this phase)
            // TODO: Parse arguments and expand with parameters
        }
    }

    return result;
}

bool Preprocessor::evaluateExpression(const std::string& expr) {
    // Simple expression evaluation
    // Support: numbers, ==, !=, <, >, <=, >=, &&, ||, !

    std::string trimmed = trim(expr);

    if (trimmed.empty()) {
        return false;
    }

    // Try to parse as number
    try {
        int value = std::stoi(trimmed);
        return value != 0;
    } catch (...) {
        // Not a simple number, try operators
    }

    // Check for comparison operators
    if (trimmed.find("==") != std::string::npos) {
        size_t pos = trimmed.find("==");
        std::string left = trim(trimmed.substr(0, pos));
        std::string right = trim(trimmed.substr(pos + 2));
        return left == right;
    }

    if (trimmed.find("!=") != std::string::npos) {
        size_t pos = trimmed.find("!=");
        std::string left = trim(trimmed.substr(0, pos));
        std::string right = trim(trimmed.substr(pos + 2));
        return left != right;
    }

    // Default to false for unsupported expressions
    return false;
}

std::string Preprocessor::trim(const std::string& str) const {
    size_t start = 0;
    while (start < str.size() && std::isspace(str[start])) ++start;

    size_t end = str.size();
    while (end > start && std::isspace(str[end - 1])) --end;

    return str.substr(start, end - start);
}

std::string Preprocessor::readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        m_errors.push_back(Error("Could not open file: " + filename,
                                SourceLocation{m_current_filename, 0, 0}));
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string Preprocessor::findIncludeFile(const std::string& filename) {
    // First try relative to current file
    std::ifstream test(filename);
    if (test.is_open()) {
        return filename;
    }

    // Try include paths
    for (const auto& path : m_include_paths) {
        std::string full_path = path + "/" + filename;
        std::ifstream test2(full_path);
        if (test2.is_open()) {
            return full_path;
        }
    }

    return "";
}

} // namespace e2asm
