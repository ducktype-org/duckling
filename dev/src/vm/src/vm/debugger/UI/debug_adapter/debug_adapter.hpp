#pragma once

#include <vm/debugger/debugger.hpp>

#include <nlohmann/json.hpp>

namespace vm::debug_adapter {

	class DebugAdapter {
	public:
		// @TODO: #2559 when load program will be available in debugger
		// change it that filepath will be get from dap message
		DebugAdapter(const fs::File& filepath);
		static DebugAdapter get(const fs::File& filepath);
		~DebugAdapter() = default;
		void run();

	private:
		events::Listener<vm::api::ProcStatus> status_change_listener;
		vm::debugger::Debugger                debugger;
		std::string                           input_buffer;

		// DAP I/O
		void send(const nlohmann::json& msg);
		void sendResponse(
			const nlohmann::json& request, bool success, const nlohmann::json& body = {}
		);
		void sendEvent(const std::string& event, const nlohmann::json& body = {});

		// Handlers
		void handleRequest(const nlohmann::json& req);
		void handleInitialize(const nlohmann::json& req);
		void handleConfigurationDone(const nlohmann::json& req);
		void handleLaunch(const nlohmann::json& req);
		void handleThreads(const nlohmann::json& req);
		void handleDisconnect(const nlohmann::json& req);
	};
}
