#pragma once

#include "configuration/configuration.hpp" // IWYU pragma: keep
#include <string_view>

namespace logger {
    /**
     * Categories of logs.
     * Feel free to extend/modify as needed.
     */
    enum class LogCategories {
        General, ///< General logs, without specific category.
        
        Lexer,   ///< Logs related to lexical analysis.
        Printer, ///< Logs related to printing operations.
        Query,   ///< Logs related to query framework.
        DVM,     ///< Logs related to the DVM component.
    };

    /**
     * Enables logging for the specified category.
     */
    void enableCategory(LogCategories category);

    namespace internal {
        /**
         * Checks if logging is enabled for the specified category.
         */
        bool isCategoryEnabled(LogCategories category);

        void logMessage(std::string_view message);
    }
}

/**
 * Macro to log user messages.
 * Usage: CORE_USER_LOG(category, message)
 * Example: CORE_USER_LOG(logger::LogCategories::General, "This is a user log message.");
 */
#define CORE_USER_LOG(category, message) \
    if (logger::enable_user_logs && logger::internal::isCategoryEnabled(category)) { \
        logger::internal::logMessage(message); \
    }


/**
 * Macro to log messages usefull mostly to developers and/or debug.
 * Usage: CORE_DEV_LOG(category, message)
 * Example: CORE_DEV_LOG(logger::LogCategories::Lexer, "This is a dev log message.");
 */
#define CORE_DEV_LOG(category, message) \
    if (logger::enable_dev_logs && logger::internal::isCategoryEnabled(category)) { \
        logger::internal::logMessage(message); \
    }

