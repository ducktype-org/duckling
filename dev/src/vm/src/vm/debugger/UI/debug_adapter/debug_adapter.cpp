#include "debug_adapter.hpp"

#include <base/collections/optional.hpp>

#include <string_id/string_id.hpp>
#include <token_source/source.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/status.hpp>
#include <vm/api/data/thread_id.hpp>
#include <vm/core/vmvalue/vmvalue.hpp>
#include <vm/debugger/UI/debug_adapter/protocol.hpp>

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

			  this->sendEvent(dap::OutputEvent(message, "console"));

			  variant_match(status) {
				  variant_case(api::Paused, status) {
					  this->sendEvent(dap::StoppedEvent("pause", 0, true));
				  }
				  variant_case(api::ExecutionCompleted, status) {
					  std::string return_str = "[";
					  bool        is_first   = true;

					  // status.exit_value
					  CORE_ASSERT(
						  std::holds_alternative<std::vector<Ref<vm::VmValue>>>(status.exit_value),
						  "Wrong variant member"
					  );
					  const auto& exit_value
						  = std::get<std::vector<Ref<vm::VmValue>>>(status.exit_value);
					  for (CRef<VmValue> val: exit_value) {
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

					  this->sendEvent(dap::OutputEvent(message, "console"));

					  this->sendEvent(dap::ExitedEvent(0));

					  this->sendEvent(dap::TerminatedEvent());
				  }
			  }
		  }),
		  output_listener([this](const std::string& str) {
			  this->sendEvent(dap::OutputEvent(str, "console"));
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
		else if (cmd == "stackTrace")
			handleStackTrace(req);
		else if (cmd == "scopes")
			handleScopes(req);
		else if (cmd == "variables")
			handleVariables(req);
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
			sendErrorResponse(req, "Unknown command");
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

	void DebugAdapter::sendErrorResponse(const nlohmann::json& request, const std::string& err_msg) {
		sendResponse(request, false, { "message", err_msg });
	}

	void DebugAdapter::sendEvent(const dap::Event& event) {
		nlohmann::json msg = { { "type", "event" }, { "event", dap::toString(event.getType()) } };

		if (event.hasBody()) msg["body"] = event.getBody();

		this->send(msg);
	}

	// ------------------- Handlers -------------------

	void DebugAdapter::handleInitialize(const nlohmann::json& req) {
		nlohmann::json capabilities = {
			{ "supportsConfigurationDoneRequest", true },
			{ "supportsSetVariable", false },
		};

		sendResponse(req, true, capabilities);

		sendEvent(dap::InitializedEvent());
	}

	void DebugAdapter::handleSetBreakpoints(const nlohmann::json& req) {
		auto        args      = req["arguments"];
		std::string file_path = args["source"]["path"].get<std::string>();

		// In DAP, client sends which breakpoints they want to have
		// Therefore, adapter should remember which one it has put
		// and add and remove breakpoint accordingly
		std::set<size_t>    incoming_lines_set;
		std::vector<size_t> incoming_lines_vector;
		if (args.contains("breakpoints"))
			for (const auto& bp: args["breakpoints"]) {
				incoming_lines_set.insert(bp["line"].get<size_t>());
				incoming_lines_vector.push_back(bp["line"].get<size_t>());
			}

		std::set<size_t>& current_lines = active_breakpoints[file_path];

		std::set<size_t> lines_to_remove;
		std::set<size_t> lines_to_add;

		std::set<size_t> lines_not_removed;


		std::ranges::set_difference(
			current_lines,
			incoming_lines_set,
			std::inserter(lines_to_remove, lines_to_remove.begin())
		);

		std::ranges::set_difference(
			incoming_lines_set, current_lines, std::inserter(lines_to_add, lines_to_add.begin())
		);

		// The response must contain the verification status of all breakpoints
		// currently requested by the client for this file.
		nlohmann::json breakpoints_json = nlohmann::json::array();

		if (deferred_launch_req.has_value()) {
			std::vector<size_t> failed_lines;
			for (size_t line: incoming_lines_vector) {
				if (lines_to_add.contains(line)) {
					auto res = debugger.setBreakpoint(fs::File(file_path), line, true);

					if (!res.has_value()) {  // The VM rejected the breakpoint, so we must not keep
						                     // it in our state
						dap::Breakpoint bp{ .verified = false,
							                .line     = line,
							                .message  = "Failed to set breakpoint: "
							                         + api::errorToString(res.error()) };
						failed_lines.push_back(line);
						breakpoints_json.push_back(bp.toJson());
						continue;
					}
				}

				dap::Breakpoint bp{ .verified = true, .line = line, .message = std::nullopt };
				breakpoints_json.push_back(bp.toJson());
			}
			// Erase after the for-loop to avoid breaking it by modifying the collection we iterate over.
			for (size_t line: failed_lines) incoming_lines_set.erase(line);

			for (size_t line: lines_to_remove) {
				auto res = debugger.setBreakpoint(fs::File(file_path), line, false);

				if (!res.has_value()) {
					// ERROR: failed to remove breakpoint
					// Inform client that it is still there
					lines_not_removed.insert(line);
				}
			}
		} else {
			// Before 'launch' (therefore before loading the target file)
			// These breakpoints will be put while handling launch request
			for (size_t line: incoming_lines_vector) {
				dap::Breakpoint bp{ .verified = false, .line = line, .message = "Before launch" };
				breakpoints_json.push_back(bp.toJson());
			}
		}

		current_lines = std::move(incoming_lines_set);

		sendResponse(req, true, { { "breakpoints", breakpoints_json } });

		if (!lines_not_removed.empty()) {
			for (size_t line: lines_not_removed) {
				dap::Breakpoint bp{ .verified = false, .line = line, .message = std::nullopt };

				this->sendEvent(dap::BreakpointEvent("changed", bp));
			}
		}
	}

	void DebugAdapter::handleConfigurationDone(const nlohmann::json& req) {
		is_configuration_done = true;
		sendResponse(req, true);

		if (deferred_launch_req.has_value()) {
			auto res = debugger.runMain();
			if (!res.has_value()) {
				std::string error_msg
					= "Failed to run main: '" + api::errorToString(res.error()) + '\'';

				sendResponse(deferred_launch_req, false, { { "message", error_msg } });
				sendEvent(dap::TerminatedEvent());
				return;
			}
			sendResponse(deferred_launch_req, true, {});
		}
	}

	void DebugAdapter::handleLaunch(const nlohmann::json& req) {
		std::string program     = req["arguments"]["program"];
		auto        load_result = debugger.loadFiles({ fs::File(program) });

		if (!load_result.has_value()) {
			sendErrorResponse(
				req,
				"Failed to load file '" + program + "': " + api::errorToString(load_result.error())
			);
			return;
		}

		deferred_launch_req = req;

		for (const auto& [file_path, lines]: active_breakpoints) {
			nlohmann::json breakpoints_json = nlohmann::json::array();

			for (size_t line: lines) {
				auto res      = debugger.setBreakpoint(fs::File(file_path), line, true);
				bool verified = res.has_value();

				dap::Breakpoint bp;
				bp.verified = verified;
				bp.line     = line;

				if (!verified)
					bp.message = "Failed to set breakpoint: " + api::errorToString(res.error());

				this->sendEvent(dap::BreakpointEvent("changed", bp));
			}
		}

		if (is_configuration_done) {
			auto res = debugger.runMain();
			if (!res.has_value()) {
				std::string error_msg = "Failed to run main: '" + api::errorToString(res.error());

				sendResponse(req, false, { { "message", error_msg } });
				sendEvent(dap::TerminatedEvent());
				return;
			}
			sendResponse(req, true, {});
		}
	}

	void DebugAdapter::handleThreads(const nlohmann::json& req) {
		nlohmann::json body
			= { { "threads",
			      nlohmann::json::array({ { { "id", 0 }, { "name", "Main Thread" } } }) } };

		sendResponse(req, true, body);
	}

	std::expected<DebugAdapter::SourcePositionInfo, std::string> DebugAdapter::getSourcePositionInfo(
	) {
		auto pos_result = debugger.getCurrentPosition();

		if (!pos_result.has_value()) {
			return std::unexpected(
				"Failed to get current position: " + api::errorToString(pos_result.error())
			);
		}

		const auto&        pos = pos_result.value();
		SourcePositionInfo info;

		if (pos.source_position.has_value()) {
			auto src          = pos.source_position.value();
			auto [sl, sc]     = src.getStartLineColumn();
			auto [el, ec]     = src.getEndLineColumn();
			info.start_line   = sl;
			info.start_column = sc;
			info.end_line     = el;
			info.end_column   = ec;

			info.file_path = std::string(src.getSource()->getFile().getFilePath().strView());
		} else {
			info.file_path  = "Function: " + std::string(pos.function_name.strView());
			info.start_line = pos.instr_number;
		}

		return info;
	}

	std::expected<std::pair<std::map<std::string, DebugAdapter::VarInfo>, base::StrID>, std::string>
		DebugAdapter::varRefFromStackFrameData(api::ThreadID thread_id, u64 frame_id) {
		auto frame_result = debugger.getStackFrameData(thread_id, frame_id);
		if (!frame_result.has_value()) return std::unexpected("Failed to get frame variables");

		auto&                          data = frame_result.value();
		std::map<std::string, VarInfo> children_map;
		std::vector<VMValueRef>        complex_children_to_register;
		u64                            next_ref = variables.size() + 1;


		for (const auto& var: data.frame_vars) {
			std::string name_str              = std::to_string(var.offset);
			if_opt_some(var.name, n) name_str = n.str();
			VarInfo info{ .var = var.value, .var_ref = 0 };

			if (var.value.isComplex()) {
				complex_children_to_register.push_back(var.value);

				info.var_ref = next_ref;
				next_ref++;

				children_map.insert({ name_str, info });

			} else {
				children_map.insert({ name_str, info });
			}
		}
		for (auto& child: complex_children_to_register) variables.emplace_back(std::move(child));


		return { { children_map, data.function_name } };
	}

	std::expected<std::map<std::string, DebugAdapter::VarInfo>, std::string> DebugAdapter::varRefFromVMValueRef(
		VMValueRef& value
	) {
		auto opt_data = value.readData();
		if (!opt_data) return std::unexpected("Failed to read data");


		namespace idv = interpreted_data_variant;
		std::map<std::string, VarInfo> children_map;

		std::vector<VMValueRef> complex_children_to_register;
		u64                     next_ref = variables.size() + 1;

		auto process_child = [&](const std::string& name, VMValueRef child) {
			VarInfo info{ .var = child, .var_ref = 0 };

			if (child.isComplex()) {
				complex_children_to_register.push_back(child);
				info.var_ref = next_ref;
				next_ref++;
			}
			children_map.insert({ name, info });
		};

		variant_match(opt_data.value()) {
			variant_case(idv::Data, data) {
				for (const auto& [subname, index]: data.field_name_map)
					process_child(subname.str(), data.fields[index].value);
			}
			variant_case(idv::Pointer, pointer) {
				if_opt_some(pointer.referenced, r) { process_child("referenced", r); }
			}
			variant_case(idv::Table, table) {
				for (usize i = 0; i < table.size; i++)
					process_child(std::to_string(i), table.get(i));
			}
			variant_case(idv::Variant, variant) { process_child("referenced", variant.referenced); }
		}
		for (auto& child: complex_children_to_register) variables.emplace_back(std::move(child));

		return children_map;
	}

	void DebugAdapter::handleStackTrace(const nlohmann::json& req) {
		std::lock_guard<std::mutex> lock(variables_mutex);
		auto thread_id     = api::ThreadID((u64) req["arguments"].value("threadId", 0));
		u64  start_frame   = (u64) req["arguments"].value("startFrame", 0);
		u64  levels        = (u64) req["arguments"].value("levels", 0);
		auto number_result = debugger.getNumberOfStackFrames(thread_id);
		if (!number_result.has_value()) {
			sendErrorResponse(
				req, "Failed to get stack trace count: " + api::errorToString(number_result.error())
			);
			return;
		}
		total_frames = number_result.value();
		for (u64 i = 0; i < total_frames; i++) variables.emplace_back(i);

		u64 requested_levels = (levels == 0) ? (total_frames - start_frame) : levels;

		u64 end_frame = (total_frames > start_frame) ? (total_frames - start_frame) : 0;

		start_frame = (end_frame >= requested_levels) ? (end_frame - requested_levels) : 0;

		nlohmann::json stack_frames = nlohmann::json::array();
		u64            i            = end_frame;
		while (i-- > start_frame) {
			auto var_ref_frame = varRefFromStackFrameData(thread_id, i);
			if (!var_ref_frame.has_value()) {
				sendErrorResponse(req, var_ref_frame.error());
				return;
			}
			auto [refs, fun_name] = var_ref_frame.value();
			variables[i]          = refs;

			auto pos_info = getSourcePositionInfo();
			if (!pos_info.has_value()) {
				sendErrorResponse(req, pos_info.error());
				return;
			}

			nlohmann::json frame
				= { { "id", i },
				    { "name", fun_name.str() },
				    { "line", pos_info->start_line },
				    { "column", pos_info->start_column },
				    { "endLine", pos_info->end_line },
				    { "endColumn", pos_info->end_column },
				    { "source",
				      { { "name", std::filesystem::path(pos_info->file_path).filename().string() },
				        { "path", pos_info->file_path } } } };

			stack_frames.push_back(frame);
		}

		nlohmann::json body = { { "stackFrames", stack_frames }, { "totalFrames", total_frames } };

		sendResponse(req, true, body);
	}

	void DebugAdapter::handleScopes(const nlohmann::json& req) {
		std::lock_guard<std::mutex> lock(variables_mutex);
		int                         frame_id = req["arguments"]["frameId"].get<int>();

		nlohmann::json response_body
			= { { "scopes",
			      nlohmann::json::array({ { { "name", "Locals" },
			                                { "variablesReference", frame_id + 1 },
			                                { "presentationHint", "arguments" },
			                                { "expensive", false } } }) } };

		sendResponse(req, true, response_body);
	}

	void DebugAdapter::handleVariables(const nlohmann::json& req) {
		std::lock_guard<std::mutex> lock(variables_mutex);
		try {
			u64 var_ref = (u64) req["arguments"]["variablesReference"].get<int>();

			if (var_ref == 0 || var_ref > variables.size()) {
				sendResponse(
					req,
					false,
					{ { "command", "variables" },
				      { "message",
				        "invalidVariablesReference: Reference " + std::to_string(var_ref)
				            + " does not exist." } }
				);
				return;
			}

			auto& current_state = variables.at(var_ref - 1);
			variant_match(current_state) {
				variant_case(u64, frame_id) {
					auto var_ref_frame = varRefFromStackFrameData(api::ThreadID(0), frame_id);
					if (!var_ref_frame.has_value()) {
						sendErrorResponse(req, var_ref_frame.error());
						return;
					}

					variables[var_ref - 1] = var_ref_frame.value().first;
				}
				variant_case(VMValueRef, value) {
					auto var_ref_value = varRefFromVMValueRef(value);
					if (!var_ref_value.has_value()) {
						sendErrorResponse(req, var_ref_value.error());
						return;
					}
					variables[var_ref - 1] = var_ref_value.value();
				}
			}
			const auto& variables_map
				= std::get<std::map<std::string, VarInfo>>(variables.at(var_ref - 1));
			nlohmann::json variables_json = nlohmann::json::array();

			for (const auto& [key, var_info]: variables_map) {
				nlohmann::json variable_item
					= { { "name", key },
					    { "value", var_info.var.str() },
					    { "type", var_info.var.getType()->getName().str() },
					    { "variablesReference", var_info.var_ref } };
				variables_json.push_back(variable_item);
			}

			sendResponse(req, true, { { "variables", variables_json } });
		} catch (const std::bad_variant_access& ex) {
			sendErrorResponse(
				req, std::string("DAP Critical Error: Bad variant access - ") + ex.what()
			);
		} catch (const nlohmann::json::exception& ex) {
			sendErrorResponse(
				req, std::string("DAP Protocol Error: Invalid JSON format - ") + ex.what()
			);
		} catch (const std::exception& ex) {
			sendErrorResponse(req, std::string("Unknown Critical Error: ") + ex.what());
		} catch (...) { sendErrorResponse(req, "Fatal unknown non-standard exception caught."); }
	}

	void DebugAdapter::handleDisconnect(const nlohmann::json& req) {
		sendResponse(req, true);
		sendEvent(dap::TerminatedEvent());
	}

	void DebugAdapter::handlePause(const nlohmann::json& req) {
		auto result = debugger.pause();

		if (!result.has_value()) {
			sendErrorResponse(req, "Failed to pause: " + api::errorToString(result.error()));
			return;
		}

		sendResponse(req, true);
	}

	void DebugAdapter::handleContinue(const nlohmann::json& req) {
		auto result = debugger.resume();
		if (!result.has_value()) {
			sendErrorResponse(req, "Failed to resume: " + api::errorToString(result.error()));
			return;
		}

		sendResponse(req, true);
	}

	void DebugAdapter::handleNext(const nlohmann::json& req) {
		auto result = debugger.step();
		if (!result.has_value()) {
			sendErrorResponse(req, "Failed to step: " + api::errorToString(result.error()));
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
