/**
 * @file exceptions.hpp
 *
 * @brief Exceptions is simple extension of standard C++ exception system.
 * It should be used everywhere.
 *
 * All exception created by us should inherit from `base::Exception`.
 * Additionally this module provides `base::Panic` exception, and `CORE_ASSERT`,
 * and `CORE_PANIC` macro, that should be used instead of things like `<cassert>`.
 */

#pragma once

#include "macros/utils.hpp"
#include "str_utils.hpp"  // IWYU pragma: export

#include <exception>
#include <string>

namespace base {

	/**
	 * Generates a stack trace, in the form of string.
	 * Currently used only in Panic, but can be usefull for debug.
	 */
	std::string getCurrentStackTrace();

	/**
	 * @brief Exception intended to replace c++ assert errors for additional functionalities.
	 */
	class Panic final: public std::exception {
		std::string position;
		std::string reason;

		std::string what_str;
		void        makeWhatStr();

	public:
		Panic(std::string position, std::string reason);

		[[nodiscard]]
		const std::string& getPosition() const;
		[[nodiscard]]
		const char* what() const noexcept final;

		// @TODO: use Printer
		void print(std::ostream& out) const;
	};

	/**
	 * @brief Exception intended to be the basis of all non-panic duckling-specific exceptions.
	 */
	class Exception: public std::exception {};

	/**
	 * @brief Duckling-specific logic error exception
	 */
	class LogicError: public Exception {
		std::string message;

	public:
		LogicError(std::string message);
		[[nodiscard]]
		const char* what() const noexcept override;
	};

	/**
	 * @brief Exception to throw in unimplemented segments.
	 */
	class NotYetImplemented: public Exception {
		std::string message;

	public:
		NotYetImplemented(const std::string& message);
		[[nodiscard]]
		const char* what() const noexcept override;
	};
}

/**
 * Base helper macro, don't use it directly.
 */
#define DETAIL_THROW_PANIC(panic_title, ...)                            \
	throw base::Panic(                                                  \
		"    In " __FILE__ ":" STRINGIFY_2(__LINE__),                   \
		base::strConcat(panic_title, "    " __VA_OPT__(, ) __VA_ARGS__) \
	)

#if defined(BUILD_TYPE_DEV)
	/**
     * @brief base::Panic based assert that allows catching for testing purposes.
     */
	#define CORE_ASSERT(cond, what, ...) \
		if (!(cond))                     \
		DETAIL_THROW_PANIC("    Assertion failed: `" #cond "`\n", what __VA_OPT__(, ) __VA_ARGS__)
#else
	/**
     * @brief base::Panic based assert that allows catching for testing purposes.
     */
	#define CORE_ASSERT(cond, what, ...) [[assume(cond)]]
#endif

/**
 * @brief base::Panic based throw that allows catching for testing purposes
 */
#define CORE_PANIC(what, ...) \
	DETAIL_THROW_PANIC("    Panic thrown:\n", what __VA_OPT__(, ) __VA_ARGS__)

/**
 * @brief Wrapper for CORE_PANIC intended to be used
 * for clearly unreachable code, e.g. after swich case where each case returns,
 * especially in cases when linter complains about missing return.
 *
 * @note CORE_PANIC is by definition also unreachable during correct execution.
 * This macro should be used instead of CORE_PANIC **only** in places where it is intuitively clear
 * that it better encapsulates the meaning/intent of the code. Examples where panic is better:
 * * `if (cond) CORE_PANIC("error description")`,
 * * `default: CORE_PANIC("unhandled case")`.
 */


#if defined(BUILD_TYPE_DEV)
	#define CORE_UNREACHABLE() DETAIL_THROW_PANIC("    Unreachable code reached! Panic.");
#else
	#define CORE_UNREACHABLE() std::unreachable();
#endif
