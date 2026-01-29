
#include <base/except/exceptions.hpp>

#include <iostream>
// #include <ostream>
#include <string_view>
#include <version>  // IWYU pragma: keep

#ifdef __cpp_lib_stacktrace
	#include "pretty_stacktrace.hpp"

	#include <stacktrace>
#endif

namespace base {

	std::string getCurrentStackTrace(u16 max_depth) {
#ifdef __cpp_lib_stacktrace
		if (max_depth > 0)
			return prettyStacktraceString(std::stacktrace::current(0, max_depth));
		else
			return prettyStacktraceString(std::stacktrace::current());
#else
		return "Stack trace is not supported in this compiler and/or system.";
#endif
	}


	namespace /* Panic state */ {
		constinit std::atomic_flag was_first_panic = false;
	}

	Panic::Panic(std::string_view position, std::string_view reason):
		  position(position),
		  reason(reason) {
		makeWhatStr();
	}

	void Panic::makeWhatStr() {
		bool am_i_first_panic = not was_first_panic.test_and_set();

		what_str.clear();
		what_str += "Unexpected compiler error occurred:\n";
		what_str += getPosition() + ":\n";
		what_str += reason + ":\n\n";
		what_str += "Stacktrace:\n";
		what_str += getCurrentStackTrace();
	}

	const std::string& Panic::getPosition() const { return position; }

	const char* Panic::what() const noexcept { return what_str.c_str(); }

	// void Panic::print(std::ostream& out) const {
	// 	out << what_str;
	// }

	void Panic::printToCerr() const { std::cerr << what_str; }

	LogicError::LogicError(std::string_view message): message(message) {}

	const char* LogicError::what() const noexcept { return message.data(); }

	NotYetImplemented::NotYetImplemented(std::string_view message):
		  message("The feature is not implemented yet: ") {
		this->message += message;
	}

	const char* NotYetImplemented::what() const noexcept { return message.data(); }

}
