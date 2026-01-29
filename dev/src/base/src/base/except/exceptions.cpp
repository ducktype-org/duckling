
#include <base/except/exceptions.hpp>

#include <iostream>
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
		/**
		 * @brief Atomic flag to indicate if a panic has already occurred.
		 * This is used to detect the first panic in the program execution.
		 */
		constinit std::atomic_flag was_first_panic = false;

		/**
		 * Storage for the reason behind the first panic occurred.
		 *
		 * @note Raw pointer is used here, as we want this code to use as little logic as possible,
		 * to avoid any possible issues during panic the handling itself.
		 */
		std::string* firstPanicWhatStr() {
			static std::string what_str;
			return &what_str;
		}
	}

	Panic::Panic(std::string_view position, std::string_view reason) {
		// note that multiple threads might race on it, and only one will win.
		// For not its ok, in the future we might want to add some per-thread first panic tracking,
		// if this becomes an issue.
		const bool am_i_first_panic = not was_first_panic.test_and_set();

		if (not am_i_first_panic) {
			what_str
				+= "======== THIS IS NOT THE FIRST PANIC IN THE PROGRAM EXECUTION! ========\n\n";
			what_str += "It most likely have been caused by panic happening durring stack unwinding OR inside another thread. ";
			what_str += "See below for the first panic details.\n\n";
		}

		what_str.clear();
		what_str += "Panic occurred:\n";

		what_str += position;
		what_str += ":\n";

		what_str += reason;
		what_str += ":\n\n";

		what_str += "Stacktrace:\n";
		what_str += getCurrentStackTrace();

		if (am_i_first_panic) {
			// store the first panic what str
			*firstPanicWhatStr() = what_str;
		} else {
			// append the first panic what str
			what_str
				+= "\n\n==================== FIRST PANIC DETAILS BELOW ====================\n\n";
			what_str += *firstPanicWhatStr();
		}
	}

	const char* Panic::what() const noexcept { return what_str.c_str(); }

	void Panic::printToCerr() const { std::cerr << what_str; }

	LogicError::LogicError(std::string_view message): message(message) {}

	const char* LogicError::what() const noexcept { return message.data(); }

	NotYetImplemented::NotYetImplemented(std::string_view message):
		  message("The feature is not implemented yet: ") {
		this->message += message;
	}

	const char* NotYetImplemented::what() const noexcept { return message.data(); }

}
