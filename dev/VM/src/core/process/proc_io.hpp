#pragma once

#include "core/thread/vmthread.hpp"

namespace vm {
	// Decisions made are based on this great article:
	// https://web.archive.org/web/20240223211016/http://wordaligned.org/articles/cpp-streambufs
	// and this: https://stackoverflow.com/questions/10150468/how-to-redirect-cin-and-cout-to-files

	class ProcIORedirecter;

	class ProcIO {
		friend class ProcIORedirecter;

	public:
		ProcIO() = default;

		/**
		 * For thread use only
		 */
		template<class T>
		T getInput(VMThread& thread) {
			T                v{};
			std::unique_lock lock(iomutex);

			// Reading from a string_stream is non-blocking, so we have to wait.
			thread.waitUntilNotPausedAndCondition(lock, [this, &thread] {
				return !thread.isAlive() || !input_stream.str().empty();
			});

			if (thread.isAlive()) input_stream >> v;
			return v;
		}

		template<class T>
		void writeOutput(const T& v) {
			std::unique_lock lock(iomutex);
			output_stream << v;
		}

		ProcIORedirecter attach(std::istream& input_source, std::ostream& output_dst);

	private:
		std::mutex iomutex;

		std::stringstream input_stream;
		std::stringstream output_stream;
	};

	/**
	 * @brief This class forwards streams to ProcIO objects.
	 */
	class ProcIORedirecter {
		friend class ProcIO;

		ProcIORedirecter(ProcIO& proc_io, std::istream& input_stream, std::ostream& output_stream):
			  input_stream(input_stream),
			  output_stream(output_stream),
			  inbuf(input_stream.rdbuf()),
			  outbuf(output_stream.rdbuf()) {
			input_stream.rdbuf(proc_io.input_stream.rdbuf());
			output_stream.rdbuf(proc_io.output_stream.rdbuf());
		}

	public:
		~ProcIORedirecter() {
			// Restore the original buffers
			input_stream.rdbuf(inbuf);
			output_stream.rdbuf(outbuf);
		}

	private:
		std::istream&   input_stream;
		std::ostream&   output_stream;
		std::streambuf* inbuf  = nullptr;
		std::streambuf* outbuf = nullptr;
	};
}
