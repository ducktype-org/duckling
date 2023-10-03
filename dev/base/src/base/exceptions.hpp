#pragma once

#include <exception>
#include <string>

#include "str_concat.hpp"

namespace base {
	// @TODO: final?
	class Panic final: public std::exception {
		std::string position;
		std::string reason;

		std::string what_str;
		void        makeWhatStr();

	public:
		Panic(std::string position, std::string reason);

		const std::string& getPosition() const;
		const char*        what() const noexcept final;

		// @TODO: use Printer
		void print(std::ostream& out) const;
	};

	class Exception: public std::exception {};

	class LogicError: public Exception {
		std::string message;

	public:
		LogicError(std::string message);
		const char* what() const noexcept override;
	};

	class NotYetImplemented: public Exception {
		std::string message;

	public:
		NotYetImplemented(std::string message);
		const char* what() const noexcept override;
	};
}  // namespace base

#define DETAIL_RIFT_STR2(X) #X
#define DETAIL_RIFT_STR(X)  DETAIL_RIFT_STR2(X)

#define RIFT_ASSERT(cond, what) \
	if (!(cond)) _THROW_PANIC("    Assertion failed: `" #cond "`\n", what)

#define RIFT_PANIC(what...) _THROW_PANIC("    Panic thrown:\n", what)

#define _THROW_PANIC(panic_title, what...)                        \
	throw base::Panic(                                            \
		"    In " __FILE__ " at line " DETAIL_RIFT_STR(__LINE__), \
		base::strConcat(panic_title, "    ", what)                \
	);
