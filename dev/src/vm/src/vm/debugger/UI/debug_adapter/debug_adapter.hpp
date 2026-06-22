#pragma once

#include <vm/debugger/debugger.hpp>

#include <nlohmann/json.hpp>

#include <mutex>

namespace vm::debugger::debug_adapter {
	/**
	 * @class DebugAdapter
	 * @brief Implements the Debug Adapter Protocol (DAP) for the VM.
	 *
	 * This class manages the lifecycle of a debugging session initiated by a DAP-compliant
	 * client (e.g., Visual Studio Code). It parses incoming JSON requests from standard input,
	 * translates them into core Debugger operations, and manages the execution flow.
	 *
	 * It acts as a thread-safe translation layer, safely emitting JSON responses and
	 * asynchronous VM status events back to the client via standard output.
	 * * @see Debugger
	 */
	class DebugAdapter {
	public:
		DebugAdapter();
		static DebugAdapter get();
		~DebugAdapter() = default;
		void run();

	private:
		events::Listener<vm::api::ProcStatus> status_change_listener;
		events::Listener<std::string>         output_listener;

		vm::debugger::Debugger debugger;
		std::string            input_buffer;
		int                    next_seq = 1;
		std::mutex             output_mutex;

		// Maps source file paths to a set of active line numbers where breakpoints are requested.
		std::map<std::string, std::set<size_t>> active_breakpoints;

		// Flag indicating whether the editor has finished sending the initial configuration.
		bool is_configuration_done = false;
		// Stores the 'launch' request to handle the asynchronous initialization handshake.
		// According to the DAP specification, 'launch' and 'configurationDone' can arrive in
		// an arbitrary order. Deferring the launch execution ensures we only spin up the VM
		// when fully configured, and fulfills VS Code's requirement that the 'launch' success
		// response must be sent last, after 'configurationDone' is processed.
		nlohmann::json deferred_launch_req = nullptr;

		// DAP I/O
		void send(const nlohmann::json& msg);
		void sendResponse(
			const nlohmann::json& request, bool success, const nlohmann::json& body = {}
		);
		void sendEvent(const std::string& event, const nlohmann::json& body = {});

		// Handlers
		void handleRequest(const nlohmann::json& req);
		void handleInitialize(const nlohmann::json& req);
		void handleSetBreakpoints(const nlohmann::json& req);
		void handleConfigurationDone(const nlohmann::json& req);
		void handleLaunch(const nlohmann::json& req);
		void handleThreads(const nlohmann::json& req);
		void handleDisconnect(const nlohmann::json& req);
		void handlePause(const nlohmann::json& req);
		void handleContinue(const nlohmann::json& req);
		void handleNext(const nlohmann::json& req);
		void handleEvaluate(const nlohmann::json& req);
	};
}
