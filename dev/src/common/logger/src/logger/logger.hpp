#pragma once

#include "module_flags/module_flags.hpp"  // IWYU pragma: keep

#include <base/pointers/ref.hpp>
#include <base/str/str_utils.hpp>  // IWYU pragma: export

#include <ostream>
#include <string_view>

namespace logger {
	/**
	 * Categories of developer logs.
	 * Feel free to extend/modify as needed.
	 * When doing so, also update the enableCategoryByStringName function.
	 */
	enum class DevLogCategories {

		// Common modules:
		Lexer,             ///< Logs related to lexical analysis.
		Printer,           ///< Logs related to printing operations.
		Artifacts,         ///< Logs related to artifacts.
		Query,             ///< Logs related to query framework.
		QueryStacktraces,  ///< Logs related to query framework stacktraces.
		NYIStacktraces,    ///< Logs related to stacktraces appended to not yet implemented
						   ///< errors/diagnostics.
		Command,           ///< Logs related to system commands.
		Diagnostics,       ///< Logs related to the diagnostic messages.

		// Compiler:
		Compiler,  ///< Logs related to compiler pipeline.
		Parser,    ///< Logs related to parsing.
		Backend,   ///< Logs related to the backend components.
		Linker,    ///< Logs related to the linker component.

		// DVM:
		DVM,          ///< Logs related to the DVM component.
		DVMDetails,   ///< Logs related to detailed logs of the DVM component.

		Incremental,  ///< Logs related to incremental compilation.

		// REPL:
		REPL,  ///< Logs related to the REPL component.
	};

	/**
	 * Enables logging for the specified category.
	 */
	void enableDevCategory(DevLogCategories category);

	/**
	 * Changes output stream used for logs.
	 * DEFAULT: stdout
	 */
	void setDevLogOutputStream(Ref<std::ostream> str);

	/**
	 * Enables logging for the specified category by its string name.
	 *
	 * @note If the category name is unknown, a warning message is printed to std::cerr.
	 */
	void enableDevCategoryByStringName(std::string_view category_name);

	namespace internal {
		void logMessage(std::string_view message);
	}

	/**
	 * Checks if logging is enabled for the specified category.
	 * @note This function is intended mostly for internal use
	 * but can also be used externally to
	 * conditionally perform some logging-like action based on log category state.
	 */
	bool isCategoryEnabled(DevLogCategories category);

	/**
	 * @brief Sets output stream to the file pointed by path.
	 */
	void setDevLogOutputFile(const std::string& path);

	/**
	 * @brief Sets output stream to the file logs/log_<timestamp>.txt
	 */
	void setDevLogOutputStreamCurrentDate();
}

/**
 * Macro to log user messages.
 * Message is evaluated only if user logs are enabled.
 *
 * Usage: CORE_USER_LOG(message)
 * Example: CORE_USER_LOG("This is a user log message.");
 *
 * @note Message is a variadic list of arguments that will be concatenated into a single string by
 * base::strConcat.
 */
#define CORE_USER_LOG(...) \
	if (logger::enable_user_logs) { logger::internal::logMessage(base::strConcat(__VA_ARGS__)); }


/**
 * Macro to log messages usefull mostly to developers and/or debug.
 * Message is evaluated only if dev logs and logs of the given category are enabled.
 *
 * Usage: CORE_DEV_LOG(category, message)
 * Example: CORE_DEV_LOG(Lexer, "This is a dev log message.");
 *
 * @note Category should be one of the enumerators of logger::DevLogCategories enum.
 * @note Message is a variadic list of arguments that will be concatenated into a single string by
 * base::strConcat.
 */
#define CORE_DEV_LOG(category, ...)                                              \
	if (::logger::enable_dev_logs) [[unlikely]] {                                \
		if (::logger::isCategoryEnabled(::logger::DevLogCategories::category)) { \
			::logger::internal::logMessage(base::strConcat(__VA_ARGS__));        \
		}                                                                        \
	}
