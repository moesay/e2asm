/**
 * @file error.h
 * @brief Error reporting infrastructure for the assembler
 *
 * Provides a unified system for collecting and formatting errors, warnings, and
 * fatal errors throughout the assembly process. All compiler phases use this to
 * report issues with precise source locations.
 */

#pragma once

#include <string>
#include <vector>
#include "../lexer/source_location.h"

namespace e2asm {

/**
 * @brief Severity level of a diagnostic message
 *
 * Warnings allow assembly to continue, errors prevent successful output,
 * and fatal errors immediately terminate processing.
 */
enum class ErrorSeverity {
    WARNING,  ///< Non-critical issue that doesn't prevent assembly
    ERROR,    ///< Problem that prevents generating valid machine code
    FATAL     ///< Critical failure that stops further processing
};

/**
 * @brief A single diagnostic message with location context
 *
 * Captures everything needed to present a helpful error to the user:
 * what went wrong, where it happened, and how serious it is.
 */
struct Error {
    std::string message;           ///< Human-readable description of the issue
    SourceLocation location;       ///< Exact position in source where error occurred
    ErrorSeverity severity;        ///< How serious this diagnostic is

    Error() : message(""), location(), severity(ErrorSeverity::ERROR) {}

    Error(std::string msg, SourceLocation loc, ErrorSeverity sev = ErrorSeverity::ERROR)
        : message(std::move(msg)), location(loc), severity(sev) {}

    /**
     * @brief Formats error in standard compiler format
     * @return String like "file.asm:10:5: error: undefined label 'start'"
     *
     * Output format matches GCC/Clang style for IDE integration.
     */
    std::string format() const {
        std::string severity_str;
        switch (severity) {
            case ErrorSeverity::WARNING: severity_str = "warning"; break;
            case ErrorSeverity::ERROR:   severity_str = "error"; break;
            case ErrorSeverity::FATAL:   severity_str = "fatal error"; break;
        }
        return location.format() + ": " + severity_str + ": " + message;
    }

    /**
     * @brief Checks if this diagnostic prevents successful assembly
     * @return true for ERROR or FATAL, false for WARNING
     */
    bool isError() const {
        return severity == ErrorSeverity::ERROR || severity == ErrorSeverity::FATAL;
    }
};

/**
 * @brief Collects errors and warnings during a compilation phase
 *
 * Each compiler phase (lexer, parser, semantic analyzer, code generator)
 * uses an ErrorReporter to accumulate diagnostics without throwing exceptions.
 * This allows the assembler to report multiple errors in one pass rather than
 * stopping at the first problem.
 *
 * The reporter tracks whether any true errors (not just warnings) have occurred,
 * which determines if assembly can proceed to the next phase.
 */
class ErrorReporter {
public:
    ErrorReporter() : m_has_errors(false) {}

    /**
     * @brief Reports a recoverable error that prevents successful assembly
     *
     * Use for problems like undefined symbols, invalid operand combinations,
     * or syntax errors. Assembly continues to find more errors but won't
     * produce output.
     *
     * @param message Description of what went wrong
     * @param location Where in the source the error occurred
     */
    void error(std::string message, SourceLocation location) {
        m_errors.emplace_back(std::move(message), location, ErrorSeverity::ERROR);
        m_has_errors = true;
    }

    /**
     * @brief Reports a potential issue that doesn't prevent assembly
     *
     * Use for suspicious but legal constructs: unused labels, ambiguous
     * operand sizes that required guessing, or deprecated syntax.
     *
     * @param message Description of the potential issue
     * @param location Where in the source the warning originated
     */
    void warning(std::string message, SourceLocation location) {
        m_errors.emplace_back(std::move(message), location, ErrorSeverity::WARNING);
    }

    /**
     * @brief Reports an unrecoverable error that stops all processing
     *
     * Use sparingly for catastrophic failures like corrupted internal state
     * or running out of memory. Most errors should be ERROR, not FATAL.
     *
     * @param message Description of the fatal problem
     * @param location Where in the source it was detected
     */
    void fatal(std::string message, SourceLocation location) {
        m_errors.emplace_back(std::move(message), location, ErrorSeverity::FATAL);
        m_has_errors = true;
    }

    /**
     * @brief Checks if any errors (not warnings) have been reported
     * @return true if error() or fatal() was called at least once
     */
    bool hasErrors() const {
        return m_has_errors;
    }

    /**
     * @brief Gets all collected diagnostics
     * @return Vector containing errors and warnings in the order they were reported
     */
    const std::vector<Error>& getErrors() const {
        return m_errors;
    }

    /**
     * @brief Resets the reporter to initial empty state
     *
     * Call between assembly runs to reuse the same reporter instance.
     */
    void clear() {
        m_errors.clear();
        m_has_errors = false;
    }

    /**
     * @brief Counts actual errors, excluding warnings
     * @return Number of ERROR and FATAL diagnostics (warnings not counted)
     */
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
    std::vector<Error> m_errors;  ///< All collected diagnostics
    bool m_has_errors;            ///< Quick check without iterating vector
};

} // namespace e2asm
