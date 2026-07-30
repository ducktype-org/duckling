#pragma once

#include <vm/debugger/debugger.hpp>

#include <mutex>

namespace vm::debugger::cli {
	/**
	 * @class CLIDebugger
	 * @brief Simple Command Line Interface Debugger
	 *
	 * This class manages the lifecycle of a debugging session in simple command line
	 * @see Debugger
	 */
	class CLIDebugger {
	public:
		CLIDebugger();
		CLIDebugger(const CLIDebugger&)            = delete;
		CLIDebugger& operator=(const CLIDebugger&) = delete;
		CLIDebugger(CLIDebugger&&)                 = delete;
		CLIDebugger& operator=(CLIDebugger&&)      = delete;
		~CLIDebugger()                             = default;

		// @TODO: #3020 Add support for multi-file debugging
		std::expected<void, api::ApiError>                            load(const fs::File& file);
		std::expected<void, std::variant<api::ApiError, std::string>> loadDefault();
		void setProgramArguments(const ProgramRunArguments& args);
		int  run();

	private:
		events::Listener<api::ProcStatus> status_change_listener;
		events::Listener<std::string>     error_listener;
		events::Listener<std::string>     output_listener;
		Debugger                          debugger;
		std::mutex                        output_mutex;

		base::Optional<fs::File>           selected_file;
		std::expected<void, api::ApiError> load_result = {};

		void printCodePosition(const CodePosition& position);

		template<typename... Args>
		void print(const Args&... content) {
			std::lock_guard lk(output_mutex);
			((std::cout << content), ...);
		}

		template<typename... Args>
		void printNL(const Args&... content) {
			std::lock_guard lk(output_mutex);
			((std::cout << content), ...);
			std::cout << "\n";
		}

		template<typename... Args>
		void printError(const Args&... content) {
			std::stringstream sstr;
			((sstr << content), ...);
			printError({ sstr.str() });
		}

		void print(const printer::PrinterContentsSeq& content);
		void printNL(const printer::PrinterContentsSeq& content);
		void printError(const printer::PrinterContentsSeq& content);
	};
}
