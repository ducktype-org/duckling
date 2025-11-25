#pragma once

#include "configuration/configuration.hpp"  // IWYU pragma: keep

#include <base/str/str_utils.hpp>           // IWYU pragma: export

#include <string_view>

namespace logger {
	/**
	 * Categories of logs.
	 * Feel free to extend/modify as needed.
	 *
	 * @note General category is enabled by default, all other categories are disabled by default.
	 */
	enum class LogCategories {
		/**
		 * General logs, without specific category.
		 * Can be also used for user logs that we expect to be always enabled.
		 */
		General,

		// Common modules:
		Lexer,        ///< Logs related to lexical analysis.
		Printer,      ///< Logs related to printing operations.
		Artifacts,    ///< Logs related to artifacts.
		Query,        ///< Logs related to query framework.
		Command,      ///< Logs related to system commands.
		Diagnostics,  ///< Logs related to the diagnostic messages.

		// Compiler:
		Compiler,  ///< Logs related to compiler pipeline.
		Backend,   ///< Logs related to the backend components.
		Linker,    ///< Logs related to the linker component.

		// DVM:
		DVM,  ///< Logs related to the DVM component.
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
 * Message is evaluated only if user logs and logs of the given category are enabled.
 *
 * Usage: CORE_USER_LOG(category, message)
 * Example: CORE_USER_LOG(General, "This is a user log message.");
 *
 * @note Category should be one of the enumerators of logger::LogCategories enum.
 * @note Message is a variadic list of arguments that will be concatenated into a single string by
 * base::strConcat.
 */
#define CORE_USER_LOG(category, ...)                                                 \
	if (logger::enable_user_logs                                                     \
	    && logger::internal::isCategoryEnabled(::logger::LogCategories::category)) { \
		logger::internal::logMessage(base::strConcat(__VA_ARGS__));                  \
	}


/**
 * Macro to log messages usefull mostly to developers and/or debug.
 * Message is evaluated only if dev logs and logs of the given category are enabled.
 *
 * Usage: CORE_DEV_LOG(category, message)
 * Example: CORE_DEV_LOG(Lexer, "This is a dev log message.");
 *
 * @note Category should be one of the enumerators of logger::LogCategories enum.
 * @note Message is a variadic list of arguments that will be concatenated into a single string by
 * base::strConcat.
 */
#define CORE_DEV_LOG(category, ...)                                                  \
	if (logger::enable_dev_logs                                                      \
	    && logger::internal::isCategoryEnabled(::logger::LogCategories::category)) { \
		logger::internal::logMessage(base::strConcat(__VA_ARGS__));                  \
	}
