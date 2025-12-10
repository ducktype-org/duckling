#include "vm_debug.hpp"

#include <iostream>
#include <string>
#include <variant>
#include "vm/api/data/api_error.hpp"
#include "vm/api/data/status.hpp"


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
	vm::FunctionRunArguments createArgumentList(DuckVMDebug::OwnedArgumentList& arguments) {
		return arguments | std::views::transform([](auto& value) { return value.refMut(); })
		     | std::ranges::to<vm::FunctionRunArguments>();
	}

	void freeArguments(const DuckVMDebug::OwnedArgumentList& arguments) {
		for (const auto& arg: arguments) arg->freeData();
	}
}

void DuckVMDebug::run() const {
	std::cout << "++++++++++++++++++++++++\n"
			     "+ BeRD has started +\n"
				 "++++++++++++++++++++++++\n";

	std::string line;
	while (true) {
		std::cout << "\n>>> ";
		if (!std::getline(std::cin, line)) {
			std::cout << "Exiting BeRD (EOF reached) or error.\n";
			break;
		}
		std::string stripped_line = strip(line);
		if (line.empty()) {
			continue;
		}
		if (stripped_line == "exit" || stripped_line == "q" || stripped_line == "quit") {
			std::cout << "Exiting BeRD\n";
			break;
		}
		if (stripped_line == "s" || stripped_line == "step") {
			step();
		} else if (stripped_line == "status") {
			getStatus();
		} else if (stripped_line == "run") {
			runVm();
		} else if (stripped_line.starts_with("run ")) {
			runFun(lstrip(lstrip(line).substr(3)));
		} else if (stripped_line == "resume") {
			resume();
		} else if (stripped_line == "pause") {
			pause();
		} else if (stripped_line == "print") {
			// print();
			throw base::NotYetImplemented("Printing not implemented yet.");
		} else if (stripped_line == "exitval" || stripped_line == "g" || stripped_line == "getexitval") {
			getExitValue();
		} else if (stripped_line == "help" || stripped_line == "?" || stripped_line == "h") {
			help();
		} else {
			std::cout << "Invalid input: \"" << line << "\"\n";
		}
	}
}

DuckVMDebug DuckVMDebug::get(const fs::File& filepath, const std::vector<std::string>& args) { return {filepath, args}; }

DuckVMDebug::DuckVMDebug() {
	const auto process_pid_response = vm::api::spawn();
	if (!process_pid_response.has_value()) throw BeRDFailedToSpawnProcessException();
	pid = process_pid_response->pid;
	if (!vm::api::attach(pid, std::cin, std::cout)) throw BeRDFailedToAttachStreamsException();
}

DuckVMDebug::DuckVMDebug(const fs::File& filepath, const std::vector<std::string>& args) : debug_args(args) {
	const auto process_pid_response = vm::api::spawn();
	if (!process_pid_response.has_value()) throw BeRDFailedToSpawnProcessException();
	pid = process_pid_response->pid;
	if (!vm::api::loadFiles(pid, { filepath })) throw BeRDFailedToLoadFile();
	std::cout<<"File loaded\n";
	if (!vm::api::attach(pid, std::cin, std::cout)) throw BeRDFailedToAttachStreamsException();
}

void DuckVMDebug::runVm() const {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value() && 
		(!std::holds_alternative<vm::api::ExecutionNotStarted>(response.value())
		 && !std::holds_alternative<vm::api::ExecutionCompleted>(response.value()))) {
		std::cout << "You have to finish execution and get exit value.\n";
		getStatus();
		return;
	}
	if (std::holds_alternative<vm::api::ExecutionCompleted>(response.value())) {
		if (!vm::api::join(pid)) throw BeRDFailedToJoinProcessException();
	}	
	if (!vm::api::run(pid, debug_args)) throw std::runtime_error("Failed to run VM");
	// getExitValue();
}

void DuckVMDebug::runFun(const std::string& string) const {
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

	u64               start = 0;
	OwnedArgumentList arguments;
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
	if (response.has_value() && 
		(!std::holds_alternative<vm::api::ExecutionNotStarted>(response.value())
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
	// getExitValue();
	// freeArguments(arguments);
}

void DuckVMDebug::getExitValue() const {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value() && !std::holds_alternative<vm::api::ExecutionCompleted>(response.value())) {
		std::cout << "No execution finished\n";
		getStatus();
		return;
	}
	// if (!vm::api::join(pid).has_value()) throw BeRDFailedToJoinProcessException();
	const auto exit_code_response = vm::api::getExitValue(pid);
	if (!exit_code_response.has_value()) throw BeRDEmptyExitCodeException();

	if (exit_code_response.value()->type->getName() != base::StrID("i64"))
		throw BeRDWrongTypeException();
	std::cout<< "Ret: " << exit_code_response.value()->readBytes<i64>() << "\n";
}

void DuckVMDebug::getStatus() const {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value()) {
		vm::api::ProcStatus status = response.value();
		std::cout << "The program is "; 
		std::visit([](auto&& arg) -> void {
			using T = std::decay_t<decltype(arg)>;
			std::cout << TypeParseTraits<T>::NAME.data() << "\n";
		}, status);
	} else {
		const vm::api::ApiError& err = response.error();
		std::cout << "error: " << vm::api::errorToString(err) << "\n";
	}
}

void DuckVMDebug::step() const {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value() && !std::holds_alternative<vm::api::Paused>(response.value())) {
		std::cout << "Not paused program - make sure you started it.\n";
		return;
	}
	if (!vm::api::step(pid)) throw BeRDFailedToMakeStep();
	// auto response = vm::api::getCurrentPosition(pid);
	// if (!response.has_value()) throw -1;
	// std::cout << response.value().function_id << " " << response.value().instr_number << "\n";
}

void DuckVMDebug::resume() const {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value() && !std::holds_alternative<vm::api::Paused>(response.value())) {
		std::cout << "Not paused program - make sure you started it.\n";
		return;
	}
	if (!vm::api::resume(pid)) throw BeRDFailedToResumeVM();
}

void DuckVMDebug::pause() const {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value() && !vm::api::isExecuting(response.value())) {
		std::cout << "Not running program right now - make sure you started it.\n";
		return;
	}
	if (!vm::api::pause(pid)) throw BeRDFailedToPauseVM();
}

void DuckVMDebug::stop() const {
	auto response = vm::api::getExecutionStatus(pid);
	if (response.has_value() && !vm::api::isExecuting(response.value())) {
		std::cout << "Not running program right now - make sure you started it.\n";
		return;
	}
	if (!vm::api::stop(pid)) throw BeRDFailedToPauseVM();
}

void DuckVMDebug::help() const {
	std::cout << "BeRD Debugger Commands:\n"
			     "  s, step              	- Execute one step in the VM\n"
			     "  run                  	- Run the VM until completion\n"
			     "  run <func([args])>   	- Run a specific function with arguments\n"
			     "  resume               	- Resume execution of the VM\n"
			     "  pause               	- Pause execution of the VM\n"
			     "  status              	- Get the current status of the VM\n"
			     "  print <var>         	- Print the value of a variable (not implemented yet)\n"
			     "  exit, q, quit       	- Exit the debugger\n"
			     "  exitval, g, getexitval  - Get the exit value of the function run in VM\n"
				 "  help, h, ?          	- Show this help message\n";
}