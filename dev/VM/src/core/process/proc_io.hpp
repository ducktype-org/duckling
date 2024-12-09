#pragma once

#include "core/thread/vmthread.hpp"
#include <iostream>

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
			std::cerr << "Getting input...\n";
			T                v{};
			std::unique_lock lock(iomutex);
			std::cout << stdio << '\n';
			if (stdio) {
				std::cerr << "Reading from cin...\n";
				std::cin >> v;
			} else {
				// Reading from a string_stream is non-blocking, so we have to wait.
				std::cerr << "Reading from input_stream...\n";
				thread.waitUntilNotPausedAndCondition(lock, [this, &thread] {
					std::cerr << "Test..." << input_stream.rdbuf()->in_avail() << '\n';
					return !thread.isAlive() || input_stream.rdbuf()->in_avail();
				});

				if (thread.isAlive()) input_stream >> v;
			}

			std::cerr << "Got input..." << v << '\n';
			return v;
		}

		template<class T>
		void writeOutput(const T& v) {
			std::unique_lock lock(iomutex);
			output_stream << v;
		}

		ProcIORedirecter attach(std::istream& input_source, std::ostream& output_dst);

	private:
		// @TODO: temporary fix, currently thread does not wake up on it's own :C
		bool stdio = false;

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
			  proc_io(proc_io),
			  input_stream(input_stream),
			  output_stream(output_stream),
			  inbuf(input_stream.rdbuf()),
			  outbuf(output_stream.rdbuf()) {
			std::cerr << "Created redirecter..." << std::endl;
			proc_io.stdio = true;
			input_stream.rdbuf(proc_io.input_stream.rdbuf());
			output_stream.rdbuf(proc_io.output_stream.rdbuf());
		}

	public:
		~ProcIORedirecter() {
			std::cerr << "Detached..." << std::endl;
			proc_io.stdio = false;
			// Restore the original buffers
			input_stream.rdbuf(inbuf);
			output_stream.rdbuf(outbuf);
		}

	private:
		ProcIO&         proc_io;
		std::istream&   input_stream;
		std::ostream&   output_stream;
		std::streambuf* inbuf  = nullptr;
		std::streambuf* outbuf = nullptr;
	};
}
