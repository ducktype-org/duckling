
#include <base/except/exceptions.hpp>

#include <atomic>
#include <iostream>
#include <mutex>
#include <string_view>
#include <version>  // IWYU pragma: keep

#ifdef __cpp_lib_stacktrace
	#include "pretty_stacktrace.hpp"

	#include <stacktrace>
#endif

namespace base {

	std::string getCurrentStackTrace(u16 max_depth) {
#ifdef __cpp_lib_stacktrace
		// Mutex to protect libbacktrace's non-thread-safe global state.
		// GCC's std::stacktrace::current() uses libbacktrace internally,
		// which has a racy mmap-based allocator (backtrace_state).
		static std::mutex stacktrace_mutex;
		std::scoped_lock  lock(stacktrace_mutex);

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
		 * @brief Atomic pointer to allocated memory that stores what() string of the first panic.
		 * @future We might want to extend this to store per-thread first panic details or simply
		 * store a lot of them (say 128 in static array), to avoid losing information about the
		 * first panic in multithreaded scenarios.
		 */
		constinit std::atomic<char*> first_panic_what_str_copy = nullptr;

		// Checks ensuring trivial destruction of the above variables.
		// This way we avoid static destruction order fiasco issues.
		static_assert(std::is_trivially_destructible_v<decltype(was_first_panic)>);
		static_assert(std::is_trivially_destructible_v<decltype(first_panic_what_str_copy)>);

		/**
		 * This class exist only to provide proper destruction of the allocated memory
		 * pointed to by first_panic_what_str_copy when the program exits.
		 */
		struct PanicWhatStrDeleter final {
			~PanicWhatStrDeleter() {
				char* ptr = first_panic_what_str_copy.load();
				first_panic_what_str_copy.store(nullptr);
				delete[] ptr;
			}
		};

		/**
		 * @brief Dummy object used to ensure proper cleanup of first panic what() string.
		 */
		constinit PanicWhatStrDeleter panic_what_str_deleter;
	}

	Panic::Panic(std::string_view position, std::string_view reason) {
		// Note that multiple threads might safely race on was_first_panic, and only one will win.
		// For not its ok, in the future we might want to add some per-thread first panic tracking,
		// if this becomes an issue.
		const bool am_i_first_panic = not was_first_panic.test_and_set();


		if (not am_i_first_panic) {
			what_str
				+= "======== THIS IS NOT THE FIRST PANIC IN THE PROGRAM EXECUTION! ========\n\n";
			what_str += "It most likely have been caused by panic happening durring stack unwinding OR inside another thread. ";
			what_str += "See below for the first panic details.\n\n";
		}

		what_str += "Panic occurred:\n";

		what_str += position;
		what_str += ":\n";

		what_str += reason;
		what_str += ":\n\n";

		what_str += "Stacktrace:\n";
		what_str += getCurrentStackTrace();

		if (am_i_first_panic) {
			// store the first panic what str
			auto size = what_str.size();

			auto buffor = new char[size + 1];
			for (size_t i = 0; i < size; i++) buffor[i] = what_str.at(i);
			buffor[size] = '\0';

			first_panic_what_str_copy.store(buffor);

		} else {
			// append the first panic what str
			what_str
				+= "\n\n==================== FIRST PANIC DETAILS BELOW ====================\n\n";

			char* first_panic_what_str_pointer = first_panic_what_str_copy.load();

			if (first_panic_what_str_pointer != nullptr) {
				what_str += first_panic_what_str_pointer;
			} else {
				what_str += "No details about the first panic are available.\n";
				what_str += "This is most likely due one of two cases:\n";
				what_str += " - The first panic happened in a different thread, and due to the timing of threads its details were not stored before another panic happened.\n";
				what_str += " - This panic happened after main() ended, and the first-panic details were already destroyed.\n";
			}
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
