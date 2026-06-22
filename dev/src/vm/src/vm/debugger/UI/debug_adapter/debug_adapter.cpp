#include "debug_adapter.hpp"

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/status.hpp>

#include <iostream>
#include <variant>

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

			  variant_match(status) {
				  variant_case(api::Paused, status) {
					  this->sendEvent(
						  "stopped",
						  { { "reason", "pause" }, { "threadId", 1 }, { "allThreadsStopped", true } }
					  );
				  }
				  variant_case(api::ExecutionCompleted, status) {
					  std::string return_str = "[";
					  bool        is_first   = true;

					  for (auto val: status.exit_value) {
						  std::string rendered_value     = "";
						  bool        has_rendered_value = false;

						  if_opt_some(val->readData(), data) {
							  variant_match(data) {
								  variant_case(vm::interpreted_data_variant::Primitive, primitive) {
									  rendered_value     = std::to_string(primitive.value);
									  has_rendered_value = true;
								  }
							  }
						  }

						  if (has_rendered_value) {
							  if (!is_first) return_str += ", ";
							  return_str += rendered_value;
							  is_first = false;
						  }
					  }
					  return_str += "]";

					  message = "VM returned: " + return_str + "\n";

					  this->sendEvent(
						  "output", { { "category", "console" }, { "output", message } }
					  );

					  this->sendEvent("exited", { { "exitCode", 0 } });

					  this->sendEvent("terminated", {});
				  }
			  }
		  }),
		  output_listener([this](const std::string& str) {
			  this->sendEvent("output", { { "category", "console" }, { "output", str } });
		  }),

		  debugger() {
		debugger.attachOnStatusChangedListener(status_change_listener);
		debugger.attachOnOutputListener(output_listener);
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
		else if (cmd == "setBreakpoints")
			handleSetBreakpoints(req);
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
		else if (cmd == "next")
			handleNext(req);
		else if (cmd == "evaluate")
			handleEvaluate(req);
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

	void DebugAdapter::handleSetBreakpoints(const nlohmann::json& req) {
		auto        args      = req["arguments"];
		std::string file_path = args["source"]["path"].get<std::string>();

		// In DAP, client sends which breakpoints they want to have
		// Therefore, adapter should remember which one it has put
		// and add and remove breakpoint accordingly
		std::set<size_t> incoming_lines;
		if (args.contains("breakpoints"))
			for (const auto& bp: args["breakpoints"])
				incoming_lines.insert(bp["line"].get<size_t>());

		std::set<size_t>& current_lines = active_breakpoints[file_path];

		std::set<size_t> lines_to_remove;
		std::set<size_t> lines_to_add;

		std::ranges::set_difference(
			current_lines, incoming_lines, std::inserter(lines_to_remove, lines_to_remove.begin())
		);

		std::ranges::set_difference(
			incoming_lines, current_lines, std::inserter(lines_to_add, lines_to_add.begin())
		);

		// The response must contain the verification status of all breakpoints
		// currently requested by the client for this file.
		nlohmann::json breakpoints_json = nlohmann::json::array();

		if (!deferred_launch_req.is_null()) {
			std::vector<size_t> failed_lines;
			for (size_t line: incoming_lines) {
				if (lines_to_add.contains(line)) {
					auto res = debugger.setBreakpoint(fs::File(file_path), line, true);

					if (!res.has_value()) {  // The VM rejected the breakpoint, so we must not keep
						                     // it in our state
						failed_lines.push_back(line);
						breakpoints_json.push_back({ { "verified", false },
						                             { "line", line },
						                             { "message",
						                               "Failed to set breakpoint: "
						                                   + api::errorToString(res.error()) } });
						continue;
					}
				}

				breakpoints_json.push_back({ { "verified", true }, { "line", line } });
			}
			// Erase after the for-loop to avoid breaking it by modifying the collection we iterate over.
			for (size_t line: failed_lines) incoming_lines.erase(line);

			for (size_t line: lines_to_remove) {
				auto res = debugger.setBreakpoint(fs::File(file_path), line, false);

				if (!res.has_value()) {
					// ERROR: failed to remove breakpoint
					// Inform client that it is still there
					incoming_lines.insert(line);

					breakpoints_json.push_back(
						{ { "verified", true },
					      { "line", line },
					      { "message",
					          "Failed to set breakpoint: " + api::errorToString(res.error()) } }
					);
				}
			}
		} else {
			// Before 'launch' (therefore before loading the target file)
			// These breakpoints will be put while handling launch request
			for (size_t line: incoming_lines)
				breakpoints_json.push_back(
					{ { "verified", false }, { "line", line }, { "message", "Before launch" } }
				);
		}

		current_lines = std::move(incoming_lines);

		sendResponse(req, true, { { "breakpoints", breakpoints_json } });
	}

	void DebugAdapter::handleConfigurationDone(const nlohmann::json& req) {
		is_configuration_done = true;
		sendResponse(req, true);

		if (!deferred_launch_req.is_null()) {
			auto res = debugger.runMain();
			if (!res.has_value()) {
				std::string error_msg = "Failed to run main: '" + api::errorToString(res.error()) + '\'';

				sendResponse(req, false, { { "message", error_msg } });
				return;
			}
			sendResponse(deferred_launch_req, true, {});
		}
	}

	void DebugAdapter::handleLaunch(const nlohmann::json& req) {
		std::string program     = req["arguments"]["program"];
		auto        load_result = debugger.loadFile(fs::File(program));

		deferred_launch_req = req;

		if (!load_result.has_value()) {
			std::string error_msg = "Failed to load file '" + program
			                      + "': " + api::errorToString(load_result.error());

			sendResponse(req, false, { { "message", error_msg } });
			return;
		}

		for (const auto& [file_path, lines]: active_breakpoints) {
			nlohmann::json breakpoints_json = nlohmann::json::array();

			for (size_t line: lines) {
				auto res      = debugger.setBreakpoint(fs::File(file_path), line, true);
				bool verified = res.has_value();

				breakpoints_json.push_back({ { "verified", verified }, { "line", line } });
			}

			sendEvent(
				"breakpoint", { { "reason", "changed" }, { "breakpoints", breakpoints_json } }
			);
		}

		if (is_configuration_done) {
			auto res = debugger.runMain();
			if (!res.has_value()) {
				std::string error_msg = "Failed to run main: '" + api::errorToString(res.error());

				sendResponse(req, false, { { "message", error_msg } });
				return;
			}
			sendResponse(req, true, {});
		}
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

	void DebugAdapter::handleNext(const nlohmann::json& req) {
		auto result = debugger.step();
		if (!result.has_value()) {
			std::string error_msg = "Failed to step: " + api::errorToString(result.error());
			sendResponse(req, false, { { "message", error_msg } });
			return;
		}

		sendResponse(req, true);
	}

	void DebugAdapter::handleEvaluate(const nlohmann::json& req) {
		auto args = req["arguments"];

		std::string user_input = args["expression"].get<std::string>();

		auto result = debugger.sendInput(user_input);

		if (!result.has_value()) {
			std::string error_msg
				= "Failed to process input: " + api::errorToString(result.error());
			sendResponse(req, false, { { "message", error_msg } });
			return;
		}
		nlohmann::json response_body
			= { { "result", "Input accepted: " + user_input }, { "variablesReference", 0 } };

		sendResponse(req, true, response_body);
	}
}
