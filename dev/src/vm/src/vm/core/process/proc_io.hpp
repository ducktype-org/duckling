#pragma once

#include <events/emitter.hpp>

#include <vm/core/thread/ivmthread.hpp>

#include <condition_variable>
#include <functional>
#include <iostream>

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
				thread.waitUntilNotPausedAndCondition(lck, [this, &thread] {
					return thread.isTerminateRequested() || input_stream.rdbuf()->in_avail()
					    || attached;
				});
			}

			if (!thread.isTerminateRequested()) input_stream >> v;

			return v;
		}

		/**
		 * Reads a single raw byte, including whitespace (unlike getInput, which uses
		 * formatted extraction and skips whitespace). Returns '\0' if terminated while waiting.
		 * For thread use only.
		 */
		char getRawChar(IVMThread& thread) {
			auto lck = lock();

			if (!attached) {
				thread.waitUntilNotPausedAndCondition(lck, [this, &thread] {
					return thread.isTerminateRequested() || input_stream.rdbuf()->in_avail()
					    || attached;
				});
			}

			char c = '\0';
			if (!thread.isTerminateRequested()) input_stream.get(c);
			return c;
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
