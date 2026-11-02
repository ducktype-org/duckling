// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <events/emitter.hpp>

#include <vm/core/thread/ivmthread.hpp>

#include <condition_variable>
#include <functional>
#include <iostream>
#include <sstream>

namespace vm {
	// Decisions made are based on this great article:
	// https://web.archive.org/web/20240223211016/http://wordaligned.org/articles/cpp-streambufs
	// and this: https://stackoverflow.com/questions/10150468/how-to-redirect-cin-and-cout-to-files

	class ProcIORedirecter;

	class ProcIO final {
		friend class ProcIORedirecter;

	public:
		ProcIO() = default;

		std::unique_lock<std::mutex> lock() { return std::unique_lock(iomutex); }

		/**
		 * For thread use only
		 */
		template<class T>
		T getInput(IVMThread& thread) {
			T    v{};
			auto lck = lock();

			// Reading from a string_stream is non-blocking, so we have to wait...
			// unless the input is redirected from std::cin, then it's blocking.
			// Also, in case of any other redirected input, all the data is usually in the buffer
			// beforehand, but it's not always true so this design is not perfect.
			if (!attached) {
				thread.waitInterruptible(lck, [this] {
					return hasPendingInput() || attached.load();
				});
			}

			if (!thread.isTerminateRequested()) input_stream >> v;

			return v;
		}

		/**
		 * Reads a single raw byte, including whitespace (unlike getInput, which uses
		 * formatted extraction and skips whitespace). Returns the byte as an int, or -1 at end
		 * of input (or if terminated while waiting), matching libc `getchar`.
		 * For thread use only.
		 */
		int getRawChar(IVMThread& thread) {
			auto lck = lock();

			if (!attached) {
				thread.waitInterruptible(lck, [this] {
					return hasPendingInput() || attached.load();
				});
			}

			if (thread.isTerminateRequested()) return -1;
			return input_stream.get();
		}

		template<class T>
		void writeOutput(const T& v) {
			{
				auto              lck = lock();
				std::stringstream sstr;
				sstr << v;
				std::string str = std::move(sstr).str();
				output_stream << str;
				output_emitter.emitEvent(str);
			}
			output_empty_cv.notify_all();
		}

		std::stringstream& inputStream() { return input_stream; }

		std::stringstream& outputStream() { return output_stream; }

		ProcIORedirecter attach(std::istream& input_source, std::ostream& output_dst);

		void attachOutputListener(Ref<events::Listener<std::string>> listener) {
			output_emitter.attachListener(listener);
		}

		std::condition_variable output_empty_cv;

	private:
		/**
		 * @brief Whether the internal input buffer holds data that has not been read yet.
		 *
		 * @note `in_avail()` cannot be used for this. It reports `egptr() - gptr()` and otherwise
		 * falls back to `showmanyc()`, which defaults to 0, and libc++'s `stringbuf` extends the
		 * get area only inside `underflow()`. Freshly written input therefore reads as "nothing
		 * available" under libc++, while libstdc++ syncs the get area eagerly and reports it.
		 * `sgetc()` goes through `underflow()`, so it sees the data on both implementations.
		 *
		 * @warning Only valid while not `attached`, i.e. while `input_stream` still owns its own
		 * buffer - on an attached buffer (`std::cin`) `sgetc()` would block.
		 */
		bool hasPendingInput() {
			if (!input_stream.good()) input_stream.clear();
			return input_stream.rdbuf()->sgetc() != std::char_traits<char>::eof();
		}

		std::atomic_bool attached = false;

		std::mutex iomutex;

		std::stringstream input_stream;
		std::stringstream output_stream;

		events::Emitter<std::string> output_emitter;
	};

	/**
	 * @brief This class forwards streams to ProcIO objects.
	 */
	class ProcIORedirecter final {
		friend class ProcIO;

		ProcIORedirecter(ProcIO& proc_io, std::istream& input_stream, std::ostream& output_stream):
			  proc_io(proc_io),
			  input(proc_io.input_stream),
			  output(proc_io.output_stream),
			  inbuf(proc_io.input_stream.rdbuf()),
			  outbuf(proc_io.output_stream.rdbuf()) {
			proc_io.attached = true;
			input.get().rdbuf(input_stream.rdbuf());
			output.get().rdbuf(output_stream.rdbuf());
		}

	public:
		~ProcIORedirecter() {
			if (!moved) {
				proc_io.get().attached = true;

				// Restore the original buffers
				input.get().rdbuf(inbuf);
				output.get().rdbuf(outbuf);
			}
		}

		ProcIORedirecter(const ProcIORedirecter&) = default;

		ProcIORedirecter(ProcIORedirecter&& other) noexcept:
			  proc_io(other.proc_io),
			  input(other.input),
			  output(other.output),
			  inbuf(other.inbuf),
			  outbuf(other.outbuf) {
			other.moved = true;
		}

		ProcIORedirecter& operator=(const ProcIORedirecter&) = delete;

		ProcIORedirecter& operator=(ProcIORedirecter&& other) noexcept {
			proc_io     = other.proc_io;
			input       = other.input;
			output      = other.output;
			inbuf       = other.inbuf;
			outbuf      = other.outbuf;
			other.moved = true;

			return *this;
		}

	private:
		bool moved = false;

		std::reference_wrapper<ProcIO> proc_io;

		std::reference_wrapper<std::istream> input;
		std::reference_wrapper<std::ostream> output;
		std::streambuf*                      inbuf  = nullptr;
		std::streambuf*                      outbuf = nullptr;
	};
}
