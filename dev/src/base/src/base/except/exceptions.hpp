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

#include <base/preproc/utils.hpp>
#include <base/str/str_utils.hpp>  // IWYU pragma: export

#include <cstring>
#include <exception>
#include <iostream>
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

	/**
	 * @brief Exception for runtime errors in system/environment checks (e.g. syscall failures).
	 * Should be used instead of Panic when the check represents a recoverable or expected
	 * system-level failure, not a programming bug.
	 */
	class RuntimeError final: public Exception {
		std::string message;

	public:
		RuntimeError(std::string_view message);

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

/**
 * Base helper macro, don't use it directly.
 */
#define DETAIL_THROW_RUNTIME_ERROR(check_title, ...)                            \
	throw base::RuntimeError(                                                   \
		base::strConcat(                                                        \
			"Runtime error occurred:\n"                                         \
			"    In " __FILE__ ":" STRINGIFY_2(__LINE__) ":\n",                 \
			check_title,                                                        \
			"    " __VA_OPT__(, ) __VA_ARGS__                                   \
		)                                                                       \
	)

// NOLINTBEGIN(concurrency-mt-unsafe)
/**
 * @brief Checks a system/OS call result in all build types.
 * On failure throws base::RuntimeError (not Panic) with the failed condition,
 * the provided message, and the current errno description via std::strerror.
 * Use this for system calls where failure is an environmental/runtime condition,
 * not a programming bug.
 */
#define CORE_SYSCALL_CHECK(cond, what, ...)                                                              \
	if (!(cond)) {                                                                                       \
		DETAIL_THROW_RUNTIME_ERROR(                                                                      \
			"    Check failed: `" #cond "`\n",                                                           \
			what, std::strerror(errno) __VA_OPT__(, ) __VA_ARGS__                                        \
		);                                                                                               \
	}
// NOLINTEND(concurrency-mt-unsafe)


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

#define CORE_ASSERT_NOEXCEPT_BASE(assert_type, cond, what, ...)                        \
	{                                                                                  \
		bool CONCAT_2(core_assert_noexcept_was_panic_, __LINE__) = false;              \
		try {                                                                          \
			assert_type(cond, what __VA_OPT__(, ) __VA_ARGS__);                        \
		} catch (const base::Panic& e) {                                               \
			e.printToCerr();                                                           \
			CONCAT_2(core_assert_noexcept_was_panic_, __LINE__) = true;                \
		}                                                                              \
		if (CONCAT_2(core_assert_noexcept_was_panic_, __LINE__)) { std::terminate(); } \
	}

#define CORE_SYSCALL_CHECK_NOEXCEPT_BASE(assert_type, cond, what, ...)                    \
	{                                                                                      \
		bool CONCAT_2(core_syscall_check_noexcept_was_error_, __LINE__) = false;           \
		try {                                                                              \
			assert_type(cond, what __VA_OPT__(, ) __VA_ARGS__);                            \
		} catch (const base::RuntimeError& e) {                                            \
			std::cerr << e.what() << '\n';                                                 \
			CONCAT_2(core_syscall_check_noexcept_was_error_, __LINE__) = true;             \
		}                                                                                  \
		if (CONCAT_2(core_syscall_check_noexcept_was_error_, __LINE__)) { std::terminate(); } \
	}

/**
 * Non throwing version of CORE_ASSERT.
 * Should be used only in places where noexcept is required.
 * When possible use CORE_ASSERT instead alongside RELEASE_NOEXCEPT if needed.
 * @note If this assertion fails the program will be terminated.
 */
#define CORE_ASSERT_NOEXCEPT(cond, what, ...) \
	CORE_ASSERT_NOEXCEPT_BASE(CORE_ASSERT, cond, what __VA_OPT__(, ) __VA_ARGS__)

/**
 * Non throwing version of CORE_SYSCALL_CHECK.
 * Should be used only in places where noexcept is required.
 * When possible use CORE_SYSCALL_CHECK instead alongside RELEASE_NOEXCEPT if needed.
 * @note If this check fails the program will be terminated.
 */
#define CORE_SYSCALL_CHECK_NOEXCEPT(cond, what, ...) \
	CORE_SYSCALL_CHECK_NOEXCEPT_BASE(CORE_SYSCALL_CHECK, cond, what __VA_OPT__(, ) __VA_ARGS__)
