#include "vm_debug_core.hpp"

#include "vm/api/data/api_error.hpp"
#include "vm/api/data/status.hpp"

#include <iostream>
#include <string>
#include <variant>

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

DuckVMDebugCore DuckVMDebugCore::get(const fs::File& filepath, const std::vector<std::string>& args) {
	return { filepath, args };
}

DuckVMDebugCore::DuckVMDebugCore() {
	if (pipe(event_pipe) < 0) throw std::runtime_error("Failed to create event pipe");
	const auto process_pid_response = vm::api::spawn();
	if (!process_pid_response.has_value()) throw BeRDFailedToSpawnProcessException();
	pid = process_pid_response->pid;
	if (!vm::api::attach(pid, std::cin, std::cout)) throw BeRDFailedToAttachStreamsException();
}

DuckVMDebugCore::DuckVMDebugCore(const fs::File& filepath, const std::vector<std::string>& args):
	  debug_args(args) {
	if (pipe(event_pipe) < 0) throw std::runtime_error("Failed to create event pipe");
	const auto process_pid_response = vm::api::spawn(event_pipe[1]);
	if (!process_pid_response.has_value()) throw BeRDFailedToSpawnProcessException();
	pid = process_pid_response->pid;
	if (!vm::api::loadFiles(pid, { filepath })) throw BeRDFailedToLoadFile();
	std::cout << "File loaded\n";
	if (!vm::api::attach(pid, std::cin, std::cout)) throw BeRDFailedToAttachStreamsException();
}

void DuckVMDebugCore::runVm() {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value()
	    && (!std::holds_alternative<vm::api::ExecutionNotStarted>(response.value())
	        && !std::holds_alternative<vm::api::ExecutionCompleted>(response.value()))) {
		std::cout << "You have to finish execution to run VM.\n";
		getStatus();
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
		std::cerr << "Error: Invalid function call -> ')' appeared before '('\n";
		return;
	}

	const std::string function_name = strip(string.substr(0, paren_open));
	if (function_name.empty()) {
		std::cerr << "Error: Invalid function call -> function name is empty\n";
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
			std::cout << "Error: Invalid argument '" << arg << "' - must be integer\n";
			return;
		}
		start = end + 1;
	}

	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value()
	    && (!std::holds_alternative<vm::api::ExecutionNotStarted>(response.value())
	        && !std::holds_alternative<vm::api::ExecutionCompleted>(response.value()))) {
		std::cout << "You have to finish execution and get exit value.\n";
		getStatus();
		return;
	}
	if (std::holds_alternative<vm::api::ExecutionCompleted>(response.value())) {
		if (!vm::api::join(pid)) throw BeRDFailedToJoinProcessException();
	}
	if (!vm::api::runFunction(pid, function_name, createArgumentList(arguments)))
		throw BeRDFailedToRunCodeException();
}

void DuckVMDebugCore::getStatus() {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value()) {
		vm::api::ProcStatus status = response.value();
		std::cout << "The program is ";
		std::visit(
			[](auto&& arg) -> void {
				using T = std::decay_t<decltype(arg)>;
				std::cout << TypeParseTraits<T>::NAME.data() << "\n";
			},
			status
		);

		if (std::holds_alternative<vm::api::ExecutionCompleted>(response.value())) {
			freeArguments(arguments);
			arguments = OwnedArgumentList();

			auto exitval = std::get<vm::api::ExecutionCompleted>(response.value()).exit_value;

			if (exitval->type->getName() != base::StrID("i64")) throw BeRDWrongTypeException();

			std::cout << "Ret: " << exitval->readBytes<i64>() << "\n";
		}
		if (std::holds_alternative<vm::api::Paused>(response.value())) {
			auto position_response = vm::api::getCurrentPosition(pid);
			if (!position_response.has_value())
				throw std::runtime_error("Failed to get current position");
			vm::api::response::CodePosition position = position_response.value();
			std::cout << "Paused on line " << position.instr_number << " of function nr "
					  << position.function_id << "\n";
		}
	} else {
		const vm::api::ApiError& err = response.error();
		std::cout << "error: " << vm::api::errorToString(err) << "\n";
	}
}

void DuckVMDebugCore::step() const {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value() && !std::holds_alternative<vm::api::Paused>(response.value())) {
		std::cout << "Not paused program - make sure you started it.\n";
		return;
	}
	if (!vm::api::step(pid)) throw BeRDFailedToMakeStep();
}

void DuckVMDebugCore::resume() const {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value() && !std::holds_alternative<vm::api::Paused>(response.value())) {
		std::cout << "Not paused program - make sure you started it.\n";
		return;
	}
	if (!vm::api::resume(pid)) throw BeRDFailedToResumeVM();
}

void DuckVMDebugCore::pause() const {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value() && !vm::api::isExecuting(response.value())) {
		std::cout << "Not running program right now - make sure you started it.\n";
		return;
	}
	if (!vm::api::pause(pid)) throw BeRDFailedToPauseVM();
}

void DuckVMDebugCore::stop() const {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value() && !vm::api::isExecuting(response.value())) {
		std::cout << "Not running program right now - make sure you started it.\n";
		return;
	}
	if (!vm::api::stop(pid)) throw BeRDFailedToPauseVM();
}

void DuckVMDebugCore::getCurrentPosition() const {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value() && !std::holds_alternative<vm::api::Paused>(response.value())) {
		std::cout << "Not paused program - make sure you started it.\n";
		return;
	}
	auto position_response = vm::api::getCurrentPosition(pid);
	if (!position_response.has_value()) throw std::runtime_error("Failed to get current position");
	vm::api::response::CodePosition position = position_response.value();
	std::cout << "Line " << position.instr_number << " of function nr " << position.function_id
			  << "\n";
}
