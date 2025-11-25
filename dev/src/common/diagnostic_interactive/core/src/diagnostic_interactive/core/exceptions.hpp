#pragma once
#include <exception>
#include <string>
#include <utility>

namespace dia_app {

    /**
     * @brief Exception intended to be the basis of all non-panic duckling-specific exceptions.
     */
    class Exception: public std::exception {};

    /**
     * @brief Exception thrown when template evaluation fails.
     */
    class TemplateEvaluationException : public Exception {
        std::string message;
    public:
        explicit TemplateEvaluationException(std::string message) : message(std::move(message)) {}
        
        [[nodiscard]] const char* what() const noexcept override {
            return message.c_str();
        }
    };

    /**
     * @brief Exception thrown when parsing a template file fails.
     */
    class ParsingTemplateFileError : public Exception {
        std::string message;
    public:
        explicit ParsingTemplateFileError(std::string message) : message(std::move(message)) {}
        
        [[nodiscard]] const char* what() const noexcept override {
            return message.c_str();
        }
    };

    /**
     * @brief Exception thrown when parsing a diagnostic file fails.
     */
    class ParsingDiagnosticFileError : public Exception {
        std::string message;
    public:
        explicit ParsingDiagnosticFileError(std::string message) : message(std::move(message)) {}
        
        [[nodiscard]] const char* what() const noexcept override {
            return message.c_str();
        }
    };

}
