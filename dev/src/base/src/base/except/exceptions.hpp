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

#include <base/preproc/cat.hpp>
#include <base/preproc/diagnostics.hpp>
#include <base/preproc/stringify.hpp>
#include <base/str/str_utils.hpp>  // IWYU pragma: export

#include <exception>
#include <string>
#include <string_view>
#include <utility>  // IWYU pragma: export

namespace base {

	/**
	 * Generates a stack trace, in the form of string.
	 * Currently used only in Panic, but can be usefull for debug.
	 */
	std::string getCurrentStackTrace(u16 max_depth = 0);

	/**
	 * @brief Exception intended to replace c++ assert errors for additional functionalities.
	 */
	class Panic final: public std::exception {
		/**
		 * @brief Full description of the panic (reason, position, stacktrace, etc).
		 */
		std::string what_str;

	public:
		Panic(std::string_view position, std::string_view reason);

		[[nodiscard]]
		const char* what() const noexcept final;

		void printToCerr() const;
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
		LogicError(std::string_view message);

		[[nodiscard]]
		const char* what() const noexcept override;
	};

	/**
	 * @brief Exception to throw in unimplemented segments.
	 */
	class NotYetImplemented final: public Exception {
		std::string message;

	public:
		NotYetImplemented(std::string_view message);

		[[nodiscard]]
		const char* what() const noexcept override;
	};

	namespace internal {
		constinit inline bool is_unit_test = false;
	}
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
	#define CORE_ASSERT(cond, what, ...) \
		PUSH_DIAGNOSTIC IGNORE_ASSUME [[assume(cond)]] POP_DIAGNOSTIC
#endif

#if defined(BUILD_TYPE_DEV)
	/**
     * @brief base::Panic based throw that allows catching for testing purposes
     */
	#define CORE_PANIC(what, ...) \
		DETAIL_THROW_PANIC("    Panic thrown:\n", what __VA_OPT__(, ) __VA_ARGS__)
#else
	#define CORE_PANIC(what, ...) std::unreachable()
#endif


#if defined(BUILD_TYPE_DEV)
	/**
     * @brief Wrapper for CORE_PANIC intended to be used
     * for clearly unreachable code, e.g. after swich case where each case returns,
     * especially in cases when linter complains about missing return.
     *
     * @note CORE_PANIC is by definition also unreachable during correct execution.
     * This macro should be used instead of CORE_PANIC **only** in places where it is intuitively
     * clear that it better encapsulates the meaning/intent of the code. Examples where panic is
     * better:
     * * `if (cond) CORE_PANIC("error description")`,
     * * `default: CORE_PANIC("unhandled case")`.
     */
	#define CORE_UNREACHABLE() DETAIL_THROW_PANIC("    Unreachable code reached! Panic.")
#else
	#define CORE_UNREACHABLE() std::unreachable()
#endif

/**
 * Non throwing version of CORE_ASSERT.
 * Should be used only in places where noexcept is required.
 * When possible use CORE_ASSERT instead alongside RELEASE_NOEXCEPT if needed.
 * @note If this assertion fails the program will be terminated.
 */
#define CORE_ASSERT_NOEXCEPT(cond, what, ...)                                     \
	{                                                                             \
		bool CAT(core_assert_noexcept_was_panic_, __LINE__) = false;              \
		try {                                                                     \
			CORE_ASSERT(cond, what __VA_OPT__(, ) __VA_ARGS__);                   \
		} catch (const base::Panic& e) {                                          \
			e.printToCerr();                                                      \
			CAT(core_assert_noexcept_was_panic_, __LINE__) = true;                \
		}                                                                         \
		if (CAT(core_assert_noexcept_was_panic_, __LINE__)) { std::terminate(); } \
	}


/**
 * @brief Panics if execution flow reaches this statement outside of a unit test.
 * Can be used to mark e.g. helper functions designed purely for testing.
 *
 * In test binaries this is a no-op, elsewhere throws a panic.
 */
#define PANIC_IF_NOT_TEST() CORE_ASSERT(base::internal::is_unit_test, "Not unit test binary")
