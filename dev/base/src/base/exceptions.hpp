#pragma once

#include <exception>
#include <string>
#include "str_utils.hpp"

namespace base {
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
	 * @brief Exception intended to be the basis of all non-panic rift-specific exceptions.
	 */
	class Exception: public std::exception {};

	/**
	 * @brief Rift-specific logic error exception
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

#define DETAIL_RIFT_STR2(X) #X
#define DETAIL_RIFT_STR(X)  DETAIL_RIFT_STR2(X)

/**
 * @brief base::Panic based assert that allows catching for testing purposes.
 */
#define RIFT_ASSERT(cond, what) \
	if (!(cond)) _THROW_PANIC("    Assertion failed: `" #cond "`\n", what)

/**
 * @brief base::Panic based throw that allows catching for testing purposes
 */
#define RIFT_PANIC(what...) _THROW_PANIC("    Panic thrown:\n", what)

#define _THROW_PANIC(panic_title, what...)                        \
	throw base::Panic(                                            \
		"    In " __FILE__ " at line " DETAIL_RIFT_STR(__LINE__), \
		base::strConcat(panic_title, "    ", what)                \
	)
