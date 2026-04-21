#include "debug_adapter.hpp"

#include <iostream>

constexpr std::string_view HEADER_PREFIX = "Content-Length: ";

DebugAdapter::DebugAdapter(const fs::File& filepath):
	  status_change_listener([this](const vm::api::ProcStatus& status) {
		  std::string message = "";
		  std::visit(
			  [&message](auto&& arg) {
				  using T = std::decay_t<decltype(arg)>;
				  message += TypeParseTraits<T>::NAME.data();
			  },
			  status
		  );

		  message += "\n";
		  this->sendEvent("output", { { "category", "console" }, { "output", message } });
	  }),
	  debugger(filepath) {
	debugger.attachOnVMStatusChangeListener(status_change_listener);
}

DebugAdapter DebugAdapter::get(const fs::File& filepath) { return { filepath }; }

void DebugAdapter::run() {
	std::string line;

	while (std::getline(std::cin, line)) {
		if (line.starts_with(HEADER_PREFIX)) {
			int length = std::stoi(line.substr(HEADER_PREFIX.length()));
			std::getline(std::cin, line);  // DAP empty line

			std::string body(length, ' ');
			std::cin.read(&body[0], length);

			try {
				nlohmann::json req = nlohmann::json::parse(body);
				if (req.value("type", "") == "request") {
					std::string cmd = req.value("command", "unknown");

					handleRequest(req);
				}
			} catch (const std::exception& e) {
				std::cerr << "[DEBUG] Exception: " << e.what() << "\n";
			}
		}
	}
}

void DebugAdapter::handleRequest(const nlohmann::json& req) {
	const std::string cmd = req.value("command", "");

	if (cmd == "initialize")
		handleInitialize(req);
	else if (cmd == "configurationDone")
		handleConfigurationDone(req);
	else if (cmd == "launch")
		handleLaunch(req);
	else if (cmd == "threads")
		handleThreads(req);
	else if (cmd == "disconnect")
		handleDisconnect(req);
	else
		sendResponse(req, false, { { "message", "Unknown command" } });
}

void DebugAdapter::send(const nlohmann::json& msg) {
	std::string body = msg.dump();
	std::cout << HEADER_PREFIX << body.size() << "\r\n\r\n" << body;
	std::cout.flush();
}

void DebugAdapter::sendResponse(const nlohmann::json& req, bool success, const nlohmann::json& body) {
	send({ { "type", "response" },
	       { "request_seq", req["seq"] },
	       { "success", success },
	       { "command", req["command"] },
	       { "body", body } });
}

void DebugAdapter::sendEvent(const std::string& event, const nlohmann::json& body) {
	send({ { "type", "event" }, { "event", event }, { "body", body } });
}

// ------------------- Handlers -------------------

void DebugAdapter::handleInitialize(const nlohmann::json& req) {
	nlohmann::json capabilities = {
		{ "supportsConfigurationDoneRequest", true },
		// @TODO: #2558 support set variable (generally memory part of debugger)
		{ "supportsSetVariable", false },
	};

	sendResponse(req, true, capabilities);

	sendEvent("initialized", nlohmann::json::object());
}

void DebugAdapter::handleConfigurationDone(const nlohmann::json& req) { sendResponse(req, true); }

void DebugAdapter::handleLaunch(const nlohmann::json& req) {
	std::string program = req["arguments"]["program"];

	// @TODO: #2559 add load program in debugger
	// for now we load program at the beggining, while constructing adapter

	sendResponse(req, true);

	// Immediately run the VM for test purpose for now
	debugger.runMain();
}

void DebugAdapter::handleThreads(const nlohmann::json& req) {
	nlohmann::json body
		= { { "threads", nlohmann::json::array({ { { "id", 1 }, { "name", "Main Thread" } } }) } };

	sendResponse(req, true, body);
}

void DebugAdapter::handleDisconnect(const nlohmann::json& req) {
	sendResponse(req, true);
	sendEvent("terminated");
}
