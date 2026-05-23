#include "debug_adapter.hpp"

#include <vm/api/data/api_error.hpp>

#include <iostream>

namespace vm::debugger::debug_adapter {
	constexpr std::string_view HEADER_PREFIX = "Content-Length: ";

	DebugAdapter::DebugAdapter():
		  status_change_listener([this](const api::ProcStatus& status) {
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

		  completion_listener([this](const vm::api::ExitValue& exit_val) {
			  std::string return_str = "[";
			  bool        is_first   = true;

			  for (auto val: exit_val) {
				  if (!is_first) return_str += ", ";
				  is_first = false;

				  if_opt_some(val->readData(), data) {
					  variant_match(data) {
						  variant_case(vm::interpreted_data_variant::Primitive, primitive) {
							  return_str += std::to_string(primitive.value);
						  }
					  }
				  }
			  }
			  return_str += "]";

			  std::string message = "VM returned: " + return_str + "\n";

			  this->sendEvent("output", { { "category", "console" }, { "output", message } });

			  this->sendEvent("exited", { { "exitCode", 0 } });

			  this->sendEvent("terminated", {});
		  }),

		  debugger() {
		debugger.attachOnStatusChangedListener(status_change_listener);
		debugger.attachOnExecutionCompletedListener(completion_listener);
	}

	DebugAdapter DebugAdapter::get() { return {}; }

	void DebugAdapter::run() {
		std::string line;

		while (std::getline(std::cin, line)) {
			if (line.starts_with(HEADER_PREFIX)) {
				int length = 0;
				try {
					length = std::stoi(line.substr(HEADER_PREFIX.length()));
				} catch (const std::exception& e) {
					std::cerr << "[DEBUG] ERROR: Invalid Content-Length format. " << e.what()
							  << "\n";
					return;
				}

				if (length < 2) {
					// The body must be valid JSON, meaning it requires at least two characters '{}'
					std::cerr << "[DEBUG] ERROR: Content-Length out of bounds: " << length << "\n";
					return;
				}

				while (std::getline(std::cin, line))
					if (line.empty() || line == "\r") break;

				std::string body;
				body.resize(static_cast<size_t>(length));

				std::cin.read(body.data(), length);

				if (std::cin.gcount() != length) {
					std::cerr
						<< "[DEBUG] ERROR: Stream ended before reading the full DAP payload.\n";
					return;
				}

				try {
					nlohmann::json req = nlohmann::json::parse(body);
					if (req.value("type", "") == "request") handleRequest(req);
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
		else if (cmd == "pause")
			handlePause(req);
		else if (cmd == "continue")
			handleContinue(req);
		else
			sendResponse(req, false, { { "message", "Unknown command" } });
	}

	void DebugAdapter::send(const nlohmann::json& msg) {
		// Mutex is needed because events can be sent by listeners from background threads
		std::lock_guard<std::mutex> lock(output_mutex);

		nlohmann::json final_msg = msg;
		final_msg["seq"]         = next_seq++;

		std::string body = final_msg.dump();
		std::cout << HEADER_PREFIX << body.size() << "\r\n\r\n" << body;
		std::cout.flush();
	}

	void DebugAdapter::sendResponse(
		const nlohmann::json& req, bool success, const nlohmann::json& body
	) {
		nlohmann::json message = { { "type", "response" },
			                       { "request_seq", req["seq"] },
			                       { "success", success },
			                       { "command", req["command"] } };

		if (!body.is_null() && !body.empty()) message["body"] = body;

		send(message);
	}

	void DebugAdapter::sendEvent(const std::string& event, const nlohmann::json& body) {
		nlohmann::json message = { { "type", "event" }, { "event", event } };

		if (!body.is_null() && !body.empty()) message["body"] = body;

		send(message);
	}

	// ------------------- Handlers -------------------

	void DebugAdapter::handleInitialize(const nlohmann::json& req) {
		nlohmann::json capabilities = {
			{ "supportsConfigurationDoneRequest", true },
			// @TODO: #2558 support set variable (generally memory part of debugger)
			{ "supportsSetVariable", false },
		};

		sendResponse(req, true, capabilities);

		sendEvent("initialized");
	}

	void DebugAdapter::handleConfigurationDone(const nlohmann::json& req) {
		sendResponse(req, true);
	}

	void DebugAdapter::handleLaunch(const nlohmann::json& req) {
		std::string program     = req["arguments"]["program"];
		auto        load_result = debugger.loadFile(fs::File(program));

		if (!load_result.has_value()) {
			std::string error_msg = "Failed to load file '" + program
			                      + "': " + api::errorToString(load_result.error());

			sendResponse(req, false, { { "message", error_msg } });
			return;
		}

		sendResponse(req, true, {});

		// Immediately run the VM for test purpose for now
		debugger.runMain();
	}

	void DebugAdapter::handleThreads(const nlohmann::json& req) {
		nlohmann::json body
			= { { "threads",
			      nlohmann::json::array({ { { "id", 1 }, { "name", "Main Thread" } } }) } };

		sendResponse(req, true, body);
	}

	void DebugAdapter::handleDisconnect(const nlohmann::json& req) {
		sendResponse(req, true);
		sendEvent("terminated");
	}

	void DebugAdapter::handlePause(const nlohmann::json& req) {
		auto result = debugger.pause();

		if (!result.has_value()) {
			std::string error_msg = "Failed to pause: " + api::errorToString(result.error());
			sendResponse(req, false, { { "message", error_msg } });
			return;
		}

		sendResponse(req, true);

		sendEvent(
			"stopped", { { "reason", "pause" }, { "threadId", 1 }, { "allThreadsStopped", true } }
		);
	}

	void DebugAdapter::handleContinue(const nlohmann::json& req) {
		auto result = debugger.resume();
		if (!result.has_value()) {
			std::string error_msg = "Failed to resume: " + api::errorToString(result.error());
			sendResponse(req, false, { { "message", error_msg } });
			return;
		}

		sendResponse(req, true);
	}
}
