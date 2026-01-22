#pragma once

#include <string>
#include <vector>
#include "../lexer/source_location.h"

namespace e2asm {

enum class ErrorSeverity {
    WARNING,
    ERROR,
    FATAL
};

struct Error {
    std::string message;
    SourceLocation location;
    ErrorSeverity severity;

    Error() : message(""), location(), severity(ErrorSeverity::ERROR) {}

    Error(std::string msg, SourceLocation loc, ErrorSeverity sev = ErrorSeverity::ERROR)
        : message(std::move(msg)), location(loc), severity(sev) {}

    std::string format() const {
        std::string severity_str;
        switch (severity) {
            case ErrorSeverity::WARNING: severity_str = "warning"; break;
            case ErrorSeverity::ERROR:   severity_str = "error"; break;
            case ErrorSeverity::FATAL:   severity_str = "fatal error"; break;
        }
        return location.format() + ": " + severity_str + ": " + message;
    }

    // checks if its a real error (not a warning)
    bool isError() const {
        return severity == ErrorSeverity::ERROR || severity == ErrorSeverity::FATAL;
    }
};

// assembe-time error collection
class ErrorReporter {
public:
    ErrorReporter() : m_has_errors(false) {}

    void error(std::string message, SourceLocation location) {
        m_errors.emplace_back(std::move(message), location, ErrorSeverity::ERROR);
        m_has_errors = true;
    }

    void warning(std::string message, SourceLocation location) {
        m_errors.emplace_back(std::move(message), location, ErrorSeverity::WARNING);
    }

    void fatal(std::string message, SourceLocation location) {
        m_errors.emplace_back(std::move(message), location, ErrorSeverity::FATAL);
        m_has_errors = true;
    }

    bool hasErrors() const {
        return m_has_errors;
    }

    const std::vector<Error>& getErrors() const {
        return m_errors;
    }

    void clear() {
        m_errors.clear();
        m_has_errors = false;
    }

    // get the errors count, warnings are execluded
    size_t errorCount() const {
        size_t count = 0;
        for (const auto& err : m_errors) {
            if (err.isError()) {
                count++;
            }
        }
        return count;
    }

private:
    std::vector<Error> m_errors;
    bool m_has_errors;
};

} // namespace e2asm
