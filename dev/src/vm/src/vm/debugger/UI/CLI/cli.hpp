#pragma once

#include <clah/clah.hpp>
#include <diagnostic/highlight_positions.hpp>

#include <vm/debugger/debugger.hpp>

#include <sstream>

namespace vm::debugger::cli {
	namespace impl {
		struct ImplementationSpecific;
	}

	/**
	 * @class CLIDebugger
	 * @brief Simple Command Line Interface Debugger
	 *
	 * This class manages the lifecycle of a debugging session in simple command line
	 * @see Debugger
	 */
	class CLIDebugger {
	public:
		CLIDebugger(const CLIDebugger&)            = delete;
		CLIDebugger& operator=(const CLIDebugger&) = delete;
		CLIDebugger(CLIDebugger&&)                 = delete;
		CLIDebugger& operator=(CLIDebugger&&)      = delete;

		CLIDebugger();
		~CLIDebugger();

		// @TODO: #3020 Add support for multi-file debugging
		std::expected<void, api::ApiError>                            load(const fs::File& file);
		std::expected<void, std::variant<api::ApiError, std::string>> loadDefault();
		void setProgramArguments(const ProgramRunArguments& args);
		int  run();

	private:
		Debugger debugger;

		base::Optional<fs::File>           selected_file;
		std::expected<void, api::ApiError> load_result = {};

		// --- Implementation specific data ---

		std::unique_ptr<impl::ImplementationSpecific> spec;

		// --- Implementation specific functions ---

		void mainLoop(clah::Clah& clah);
		void exitMainLoop();
		void print(const printer::PrinterContentsSeq& content);

		// --- Implementation independent print generalisation ---

		void printCodePosition(const CodePosition& position);

		template<typename... Args>
		void print(const Args&... content) {
			std::stringstream sstr;
			((sstr << content), ...);
			print({ sstr.str() });
		}

		template<typename... Args>
		void printNL(const Args&... content) {
			std::stringstream sstr;
			((sstr << content), ...);
			printNL({ sstr.str() });
		}

		template<typename... Args>
		void printError(const Args&... content) {
			std::stringstream sstr;
			((sstr << content), ...);
			printError({ sstr.str() });
		}

		void printNL(const printer::PrinterContentsSeq& content);
		void printError(const printer::PrinterContentsSeq& content);
	};
}
