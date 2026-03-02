#include "vm_debug_core.hpp"

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/status.hpp>

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <variant>

namespace vm::debugger {
	namespace {
		Box<vm::VmValue> getIntVmValue(const vm::PID pid, const i64 value) {
			auto response = vm::api::getVmValue(pid, "i64");
			if (!response.has_value()) throw BeRDFailedToCreateAVmValue();
			auto vm_value = std::move(response->vm_value);
			vm_value->writeBytes<i64>(value);
			return vm_value;
		}

		std::string strip(std::string string) {
			string.erase(0, string.find_first_not_of(" \t\n\r"));
			string.erase(string.find_last_not_of(" \t\n\r") + 1);
			return string;
		}

		std::string lstrip(std::string string) {
			string.erase(0, string.find_first_not_of(" \t\n\r"));
			return string;
		}

		/**
		 * @brief Create a list of references to owned VmValues which can be passed to the VM.
		 */
		vm::FunctionRunArguments createArgumentList(DuckVMDebugCore::OwnedArgumentList& arguments) {
			return arguments | std::views::transform([](auto& value) { return value.refMut(); })
			     | std::ranges::to<vm::FunctionRunArguments>();
		}

		void freeArguments(const DuckVMDebugCore::OwnedArgumentList& arguments) {
			for (const auto& arg: arguments) arg->freeData();
		}
	}

	DuckVMDebugCore DuckVMDebugCore::get(
		const fs::File& filepath, const std::vector<std::string>& args
	) {
		return { filepath, args };
	}

	DuckVMDebugCore::DuckVMDebugCore(): DuckVMDebugCore(std::cin, std::cout) {}

	DuckVMDebugCore::DuckVMDebugCore(std::istream& vm_input_stream, std::ostream& vm_output_stream) {
		if (pipe(event_pipe) < 0) throw std::runtime_error("Failed to create event pipe");
		const auto process_pid_response = vm::api::spawn();
		if (!process_pid_response.has_value()) throw BeRDFailedToSpawnProcessException();
		pid = process_pid_response->pid;
		if (!vm::api::attach(pid, vm_input_stream, vm_output_stream))
			throw BeRDFailedToAttachStreamsException();
	}

	DuckVMDebugCore::DuckVMDebugCore(const fs::File& filepath, const std::vector<std::string>& args):
		  DuckVMDebugCore(std::cin, std::cout, filepath, args) {}

	DuckVMDebugCore::DuckVMDebugCore(
		std::istream&                   vm_input_stream,
		std::ostream&                   vm_output_stream,
		const fs::File&                 filepath,
		const std::vector<std::string>& args
	):
		  debug_args(args) {
		if (pipe(event_pipe) < 0) throw std::runtime_error("Failed to create event pipe");
		const auto process_pid_response = vm::api::spawn(event_pipe[1]);
		if (!process_pid_response.has_value()) throw BeRDFailedToSpawnProcessException();
		pid = process_pid_response->pid;
		if (!vm::api::loadFiles(pid, { filepath })) throw BeRDFailedToLoadFile();
		if (!vm::api::attach(pid, vm_input_stream, vm_output_stream))
			throw BeRDFailedToAttachStreamsException();
	}

	void DuckVMDebugCore::runVm() {
		auto response = vm::api::getExecutionStatus(pid);
		if (response.has_value()
		    && (!std::holds_alternative<vm::api::ExecutionNotStarted>(response.value())
		        && !std::holds_alternative<vm::api::ExecutionCompleted>(response.value()))) {
			onMessage.handleEvent("You have to finish execution to run VM.");
			// getStatus();
			return;
		}
		if (std::holds_alternative<vm::api::ExecutionCompleted>(response.value())) {
			if (!vm::api::join(pid)) throw BeRDFailedToJoinProcessException();
		}
		if (!vm::api::run(pid, debug_args)) throw std::runtime_error("Failed to run VM");
	}

	void DuckVMDebugCore::runFun(const std::string& string) {
		const u64 paren_open  = string.find('(');
		const u64 paren_close = string.rfind(')');

		if (paren_open == std::string::npos || paren_close == std::string::npos
		    || paren_close <= paren_open) {
			onMessage.handleEvent("Error: Invalid function call -> ')' appeared before '('");
			return;
		}

		const std::string function_name = strip(string.substr(0, paren_open));
		if (function_name.empty()) {
			onMessage.handleEvent("Error: Invalid function call -> function name is empty");
			return;
		}

		if (function_name == "main") {
			runVm();
			return;
		}

		std::string args_str = string.substr(paren_open + 1, paren_close - paren_open - 1);

		u64 start = 0;
		arguments = OwnedArgumentList();
		while (start < args_str.length()) {
			u64 end = args_str.find(',', start);

			// Last argument.
			if (end == std::string::npos) end = args_str.length();

			std::string arg = strip(args_str.substr(start, end - start));
			try {
				if (!arg.empty()) arguments.push_back(getIntVmValue(pid, std::stoll(arg)));
			} catch (const std::exception& _) {
				onMessage.handleEvent("Error: Invalid argument '" + arg + "' - must be integer");
				return;
			}
			start = end + 1;
		}

		auto response = vm::api::getExecutionStatus(pid);
		if (response.has_value()
		    && (!std::holds_alternative<vm::api::ExecutionNotStarted>(response.value())
		        && !std::holds_alternative<vm::api::ExecutionCompleted>(response.value()))) {
			onMessage.handleEvent("You have to finish execution and get exit value.");
			// getStatus();
			return;
		}
		if (std::holds_alternative<vm::api::ExecutionCompleted>(response.value())) {
			if (!vm::api::join(pid)) throw BeRDFailedToJoinProcessException();
		}
		if (!vm::api::runFunction(pid, function_name, createArgumentList(arguments)))
			throw BeRDFailedToRunCodeException();
	}

	vm::api::ProcStatus DuckVMDebugCore::getStatus() {
		clearEnumeratedVariablesReferences();

		auto response = vm::api::getExecutionStatus(pid);
		if (response.has_value()) {
			vm::api::ProcStatus status = response.value();

			onVmStateChange.handleEvent(status);

			if (std::holds_alternative<vm::api::ExecutionCompleted>(response.value())) {
				freeArguments(arguments);
				arguments = OwnedArgumentList();

				auto exitval = std::get<vm::api::ExecutionCompleted>(response.value()).exit_value;

				onVmExecutionCompleted.handleEvent(exitval);
			}
			if (std::holds_alternative<vm::api::Paused>(response.value())) {
				auto position_response = vm::api::getCurrentPosition(pid);
				if (!position_response.has_value())
					throw std::runtime_error("Failed to get current position");
				vm::api::response::CodePosition position = position_response.value();

				onVmExecutionPaused.handleEvent(position);
			}
			return status;
		} else {
			const vm::api::ApiError& err = response.error();
			onMessage.handleEvent("error: " + vm::api::errorToString(err));
			// TODO: change it for sth better
			return vm::api::Paused();
		}
	}

	void DuckVMDebugCore::step() const {
		auto response = vm::api::getExecutionStatus(pid);
		if (response.has_value() && !std::holds_alternative<vm::api::Paused>(response.value())) {
			onMessage.handleEvent("Not paused program - make sure you started it.");
			return;
		}
		if (!vm::api::step(pid)) throw BeRDFailedToMakeStep();
	}

	void DuckVMDebugCore::resume() const {
		auto response = vm::api::getExecutionStatus(pid);
		if (response.has_value() && !std::holds_alternative<vm::api::Paused>(response.value())) {
			onMessage.handleEvent("Not paused program - make sure you started it.");
			return;
		}
		if (!vm::api::resume(pid)) throw BeRDFailedToResumeVM();
	}

	void DuckVMDebugCore::pause() const {
		auto response = vm::api::getExecutionStatus(pid);
		if (response.has_value() && !vm::api::isExecuting(response.value())) {
			onMessage.handleEvent("Not running program right now - make sure you started it.");
			return;
		}
		if (!vm::api::pause(pid)) throw BeRDFailedToPauseVM();
	}

	void DuckVMDebugCore::stop() const {
		auto response = vm::api::getExecutionStatus(pid);
		if (response.has_value() && !vm::api::isExecuting(response.value())) {
			onMessage.handleEvent("Not running program right now - make sure you started it.");
			return;
		}
		if (!vm::api::stop(pid)) throw BeRDFailedToPauseVM();
	}

	void DuckVMDebugCore::getCurrentPosition() const {
		auto response = vm::api::getExecutionStatus(pid);
		if (response.has_value() && !std::holds_alternative<vm::api::Paused>(response.value())) {
			onMessage.handleEvent("Not paused program - make sure you started it.");
			return;
		}
		auto position_response = vm::api::getCurrentPosition(pid);
		if (!position_response.has_value())
			throw std::runtime_error("Failed to get current position");
		vm::api::response::CodePosition position = position_response.value();
		onVmExecutionPaused.handleEvent(position);
	}

	void DuckVMDebugCore::printMemory() const {
		auto response_nosf = vm::api::debuggerGetNumberOfStackFrames(pid);
		if (!response_nosf.has_value()) {
			std::cerr << "get number of stack frames error";
			return;
		}

		u64 number_of_stack_frames = response_nosf.value().number_of_stack_frames;
		for (u64 frame_index = 0; frame_index < number_of_stack_frames; frame_index++) {
			auto response_sfv = vm::api::debuggerGetStackFrameVars(pid, frame_index);
			if (!response_sfv.has_value()) {
				std::cerr << "get number of stack frames error";
				return;
			}

			std::cout << "Frame " << frame_index << " ("
					  << response_sfv.value().function_name.strView() << ")\n";

			auto vars = response_sfv.value().frame_vars;
			for (auto& var: vars) {
				std::cout << "\t+ 0x" << std::hex << var.offset << " aka. " << std::dec
						  << var.offset << " <" << var.type->getName().strView() << "> [size: 0x"
						  << std::hex << var.type->getSize() << std::dec << "]:";
				auto response_pd
					= vm::api::debuggerGetPointerData(pid, var.pointer, var.type->getSize());
				if (!response_pd.has_value()) {
					std::cerr << "get pointer data error";
					return;
				}
				base::ModRawView var_data_view = response_pd.value().data;

				std::cout << " (mem addr btw: " << var_data_view.getBegin() << ") ";


				switch (var.type->getKind()) {
				case vm::Type::Kind::None:
					break;
				case vm::Type::Kind::Primitive:
					if (var_data_view.size() == 1) {
						u8 value = vm::safeReadPointerBytes<u8>(var_data_view.getBegin());
						std::cout << " " << (u16) value << "\n";
					}
					if (var_data_view.size() == 2) {
						u16 value = vm::safeReadPointerBytes<u16>(var_data_view.getBegin());
						std::cout << " " << value << "\n";
					}
					if (var_data_view.size() == 4) {
						u32 value = vm::safeReadPointerBytes<u32>(var_data_view.getBegin());
						std::cout << " " << value << "\n";
					}
					if (var_data_view.size() == 8) {
						u64 value = vm::safeReadPointerBytes<u64>(var_data_view.getBegin());
						std::cout << " " << value << "\n";
					}

					break;
				case vm::Type::Kind::Pointer:
					std::cout << " pointer\n";
					break;
				// FixedSizeTable
				// DynamicTable
				// Data
				// Variant
				// Function
				// Opaque
				default:
					for (u64 i = 0; i < var_data_view.size(); i++) {
						std::cout << " 0x" << std::hex << std::setfill('0') << std::setw(2)
								  << std::to_integer<u64>(var_data_view.getBegin()[i]);
					}
					std::cout << std::dec << "\n";
					break;
				}
			}
		}
	}

	void DuckVMDebugCore::clearEnumeratedVariablesReferences() {
		enumerated_variables_references.clear();
		enumerated_variables_references.push_back(VariablesReference{
			.id = 0, .vr = VariablesReference::Nothing{} });
	}

	std::vector<DuckVMDebugCore::StackFrameInfo> DuckVMDebugCore::enumerateFrames(u64 thread_id) {
		auto response_nosf = vm::api::debuggerGetNumberOfStackFrames(pid);
		if (!response_nosf.has_value()) {
			onMessage.handleEvent("get number of stack frames error");
			return {};
		}

		u64 number_of_stack_frames = response_nosf.value().number_of_stack_frames;
		std::vector<StackFrameInfo> frames_info;
		for (u64 frame_index = 0; frame_index < number_of_stack_frames; frame_index++) {
			auto response_sfv = vm::api::debuggerGetStackFrameVars(pid, frame_index);
			if (!response_sfv.has_value()) {
				onMessage.handleEvent("get stack frame vars error");
				return {};
			}

			// check if we already have reference to this frame
			auto it = std::find_if(
				enumerated_variables_references.begin(),
				enumerated_variables_references.end(),
				[thread_id, frame_index](const VariablesReference& vr) {
					return std::holds_alternative<StackFrameHook>(vr.vr)
				        && std::get<StackFrameHook>(vr.vr).thread_id == thread_id
				        && std::get<StackFrameHook>(vr.vr).frame_id == frame_index;
				}
			);

			if (it != enumerated_variables_references.end()) {
				frames_info.push_back(StackFrameInfo{ .frame_id = frame_index,
				                                      .function_name
				                                      = response_sfv.value().function_name,
				                                      .variables_reference = it->id });
			} else {
				u64 variables_reference
					= enumerated_variables_references.size();  // TODO: rethink numerating

				frames_info.push_back(StackFrameInfo{ .frame_id = frame_index,
				                                      .function_name
				                                      = response_sfv.value().function_name,
				                                      .variables_reference = variables_reference });

				enumerated_variables_references.push_back(VariablesReference{
					.id = variables_reference,
					.vr = StackFrameHook{ .thread_id = thread_id, .frame_id = frame_index } });
			}
		}
		return frames_info;
	}

	std::string valueToString(const vm::PID pid, const vm::Pointer pointer, const vm::TypeCRef type) {
		auto response_pd = vm::api::debuggerGetPointerData(pid, pointer, type->getSize());
		if (!response_pd.has_value()) {
			// onMessage.handleEvent("get pointer data error");
			return "<error>";
		}
		base::ModRawView var_data_view = response_pd.value().data;

		switch (type->getKind()) {
		case vm::Type::Kind::None:
			return "<none>";
		case vm::Type::Kind::Primitive: {
			if (var_data_view.size() == 1) {
				u8 value = vm::safeReadPointerBytes<u8>(var_data_view.getBegin());
				return std::to_string((u16) value);
			}
			if (var_data_view.size() == 2) {
				u16 value = vm::safeReadPointerBytes<u16>(var_data_view.getBegin());
				return std::to_string(value);
			}
			if (var_data_view.size() == 4) {
				u32 value = vm::safeReadPointerBytes<u32>(var_data_view.getBegin());
				return std::to_string(value);
			}
			if (var_data_view.size() == 8) {
				u64 value = vm::safeReadPointerBytes<u64>(var_data_view.getBegin());
				return std::to_string(value);
			}
			return "<unsupported primitive type>";
		};
		case vm::Type::Kind::Pointer: {
			vm::Pointer value = vm::safeReadPointerBytes<vm::Pointer>(var_data_view.getBegin());
			if (value.isNull()) return "null";
		}
			return "<pointer>";
		default:
			return "<unsupported value>";  // TODO: implement toString for other types
		}
	}

	std::vector<DuckVMDebugCore::VariableInfo> DuckVMDebugCore::dereferenceVariablesReference(
		u64 variables_reference
	) {
		auto it = std::find_if(
			enumerated_variables_references.begin(),
			enumerated_variables_references.end(),
			[variables_reference](const VariablesReference& vr) {
				return vr.id == variables_reference;
			}
		);

		if (it == enumerated_variables_references.end())
			throw std::runtime_error("Invalid variables reference");

		std::vector<VariableInfo> variables_info;

		auto var_vars_ref = [&](vm::api::response::StackFrameVars::FrameVar var) -> u64 {
			u64 var_ref = 0;

			switch (var.type->getKind()) {
			case vm::Type::Kind::None:
			case vm::Type::Kind::Primitive:
			case vm::Type::Kind::Opaque:
			case vm::Type::Kind::Function:
				break;
			case vm::Type::Kind::Pointer:
			case vm::Type::Kind::FixedSizeTable:
			case vm::Type::Kind::DynamicTable:
			case vm::Type::Kind::Data:
			case vm::Type::Kind::Variant:
				// check if we already have reference to this variable
				auto existing_vr = std::find_if(
					enumerated_variables_references.begin(),
					enumerated_variables_references.end(),
					[&var](const VariablesReference& vr) {
						return std::holds_alternative<VariableHook>(vr.vr)
					        && std::get<VariableHook>(vr.vr).pointer == var.pointer
					        && std::get<VariableHook>(vr.vr).type == var.type;
					}
				);

				if (existing_vr == enumerated_variables_references.end()) {
					var_ref = enumerated_variables_references.size();  // TODO: rethink numerating
					enumerated_variables_references.push_back(VariablesReference{
						.id = var_ref,
						.vr = VariableHook{ .pointer = var.pointer, .type = var.type } });
					std::stringstream ss;
					ss << "Created new variables reference " << var_ref
					   << " for variable at offset 0x" << std::hex << var.offset << std::dec
					   << " of type " << var.type->getName().strView();
					onMessage.handleEvent(ss.str());
				} else {
					var_ref = existing_vr->id;
					std::stringstream ss;
					ss << "Reusing existing variables reference " << var_ref
					   << " for variable at offset 0x" << std::hex << var.offset << std::dec
					   << " of type " << var.type->getName().strView();
					onMessage.handleEvent(ss.str());
				}
				break;
			}

			return var_ref;
		};

		variant_match(it->vr) {
			variant_case(StackFrameHook, hook) {
				auto response_sfv = vm::api::debuggerGetStackFrameVars(pid, hook.frame_id);
				if (!response_sfv.has_value()) {
					onMessage.handleEvent("get stack frame vars error");
					return {};
				}

				for (auto& var: response_sfv.value().frame_vars) {
					variables_info.push_back(VariableInfo{
						.name                = base::StrID("var" + std::to_string(var.offset)),
						.value               = valueToString(pid, var.pointer, var.type),
						.type                = var.type->getName().str(),
						.variables_reference = var_vars_ref(var),
					});
				}
			}
			variant_case(VariableHook, hook) {
				throw std::runtime_error("Dereferencing variable hook is not implemented yet");
			}
			variant_case_novalue(VariablesReference::Nothing) {
				onMessage.handleEvent("This variables reference does not refer to anything\n");
				return {};
			}
		}

		return variables_info;
	}

	void DuckVMDebugCore::editVariable(
		u64 variables_reference, const std::string& variable_name, const std::string& new_value
	) {
		// find variable
		auto it = std::find_if(
			enumerated_variables_references.begin(),
			enumerated_variables_references.end(),
			[variables_reference](const VariablesReference& vr) {
				return vr.id == variables_reference;
			}
		);
		if (it == enumerated_variables_references.end())
			throw std::runtime_error("Invalid variables reference");

		variant_match(it->vr) {
			variant_case(StackFrameHook, hook) {
				// find variable with given name in this frame
				auto response_sfv = vm::api::debuggerGetStackFrameVars(pid, hook.frame_id);
				if (!response_sfv.has_value()) {
					onMessage.handleEvent("get stack frame vars error");
					return;
				}
				auto var_it = std::find_if(
					response_sfv.value().frame_vars.begin(),
					response_sfv.value().frame_vars.end(),
					[&variable_name](const vm::api::response::StackFrameVars::FrameVar& var) {
						return ("var" + std::to_string(var.offset)) == variable_name;
					}
				);
				if (var_it == response_sfv.value().frame_vars.end()) {
					onMessage.handleEvent(
						"Variable with name '" + variable_name + "' not found in this frame\n"
					);
					return;
				}

				// currently only supports editing primitive types by rewriting their bytes
				auto& var = *var_it;
				if (var.type->getKind() != vm::Type::Kind::Primitive) {
					onMessage.handleEvent(
						"Editing variable of type '" + var.type->getName().str()
						+ "' is not supported yet\n"
					);
					return;
				}

				// get memory view write new value bytes to VM memory
				auto response_pd
					= vm::api::debuggerGetPointerData(pid, var.pointer, var.type->getSize());
				if (!response_pd.has_value()) {
					onMessage.handleEvent("get pointer data error");
					return;
				}
				base::ModRawView var_data_view = response_pd.value().data;

				// parse new value according to variable type
				try {
					if (var.type->getSize() == 1) {
						u8 value = static_cast<u8>(std::stoul(new_value));
						std::memcpy(var_data_view.getBegin(), &value, 1);
					} else if (var.type->getSize() == 2) {
						u16 value = static_cast<u16>(std::stoul(new_value));
						std::memcpy(var_data_view.getBegin(), &value, 2);
					} else if (var.type->getSize() == 4) {
						u32 value = static_cast<u32>(std::stoul(new_value));
						std::memcpy(var_data_view.getBegin(), &value, 4);
					} else if (var.type->getSize() == 8) {
						u64 value = static_cast<u64>(std::stoull(new_value));
						std::memcpy(var_data_view.getBegin(), &value, 8);
					} else {
						onMessage.handleEvent(
							"Unsupported primitive type size: " + std::to_string(var.type->getSize())
						);
						return;
					}
				} catch (const std::exception& e) {
					onMessage.handleEvent("Failed to parse new value: " + std::string(e.what()));
					return;
				}
			}
			variant_default {
				onMessage.handleEvent("Editing variable hook is not implemented yet");
				return;
			}
		}
	}
}
