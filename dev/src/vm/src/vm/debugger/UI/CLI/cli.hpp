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
		CLIDebugger(const std::vector<std::string>& main_args = {});
		CLIDebugger(const fs::File& filepath, const std::vector<std::string>& main_args = {});
		CLIDebugger(const CLIDebugger&)            = delete;
		CLIDebugger& operator=(const CLIDebugger&) = delete;
		CLIDebugger(CLIDebugger&&)                 = delete;
		CLIDebugger& operator=(CLIDebugger&&)      = delete;
		~CLIDebugger()                             = default;

		int run();

	private:
		events::Listener<api::ProcStatus> status_change_listener;
		events::Listener<std::string>     error_listener;
		Debugger                          debugger;
		std::mutex                        output_mutex;

		base::Optional<fs::File>           selected_file;
		std::expected<void, api::ApiError> load_result = {};

		void help();
		void status();
		void position();
	};
}
