#include "vm_debug.hpp"

#include <iostream>
#include <string>


namespace {
	Box<vm::VmValue> getIntVmValue(vm::PID pid, i64 value) {
		auto response = vm::api::getVmValue(pid, "i64");
		if (!response.has_value()) throw -1;
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

	void freeArguments(DuckVMDebug::OwnedArgumentList& arguments) {
		for (auto& arg: arguments) arg->freeData();
	}
}

void DuckVMDebug::run() {
	std::cout << "++++++++++++++++++++++++\n+ BeRD has "
				 "started +\n++++++++++++++++++++++++\n";

	std::string line;
	while (true) {
		std::cout << "\n>>> ";
		if (!std::getline(std::cin, line)) {
			std::cout << "Exiting BeRD (EOF reached) or error.\n";
			break;
		}
		std::string stripped_line = strip(line);
		if (line == "") {
			continue;
		} else if (stripped_line == "exit" || stripped_line == "q") {
			std::cout << "Exiting BeRD\n";
			break;
		} else if (stripped_line == "s" || stripped_line == "step") {
			step();
		} else if (stripped_line == "status") {
			get_status();
		} else if (stripped_line == "run") {
			run_vm();
		} else if (stripped_line.starts_with("run ")) {
			run_fun(lstrip(lstrip(line).substr(3)));
		} else if (stripped_line == "resume") {
			resume();
		} else if (stripped_line == "pause") {
			pause();
		} else if (stripped_line == "print") {
			// print();
			throw -1;
		} else if (lstrip(line).starts_with("$")) {
			// processGlobalOutput(line);
		// } else if (lstrip(line).starts_with("!")) {
		// 	// processExecuteInstruction(line);
		// } else if (line.find('(') != std::string::npos && line.find(')') != std::string::npos
		//            && line.find('(') < line.find(')')) {
		// 	// processFunctionCall(line);
		} else {
			std::cout << "Invalid input: \"" << line << "\"\n";
		}
	}
}

DuckVMDebug DuckVMDebug::get(const fs::File& filepath, const std::vector<std::string>& args) { return {filepath, args}; }

DuckVMDebug::DuckVMDebug() {
	auto process_pid_response = vm::api::spawn();
	if (!process_pid_response.has_value()) throw -1;
	pid = process_pid_response->pid;
	if (!vm::api::attach(pid, std::cin, std::cout)) throw -1;
}

DuckVMDebug::DuckVMDebug(const fs::File& filepath, const std::vector<std::string>& args) {
	debug_args = args;
	auto process_pid_response = vm::api::spawn();
	if (!process_pid_response.has_value()) throw -1;
	pid = process_pid_response->pid;
	if (!vm::api::loadFiles(pid, { filepath })) throw -1;
	std::cout<<"File loaded\n";
	if (!vm::api::attach(pid, std::cin, std::cout)) throw -1;
}

void DuckVMDebug::run_vm(){

	if (!vm::api::run(pid, debug_args)) throw std::runtime_error("Failed to run VM");
	get_exit_value();
}

void DuckVMDebug::run_fun(const std::string& string){

	u64 paren_open  = string.find('(');
	u64 paren_close = string.rfind(')');

	if (paren_open == std::string::npos || paren_close == std::string::npos
	    || paren_close <= paren_open) {
		std::cerr << "Error: Invalid function call -> ')' appeared before '('\n";
		return;
	}


	std::string function_name = strip(string.substr(0, paren_open));
	if (function_name.empty()) {
		std::cerr << "Error: Invalid function call -> function name is empty\n";
		return;
	}

	if (function_name == "main") {
		run_vm();
		// std::cerr << "Error: Calling 'main' function directly is not supported\n";
		return;
	}

	std::string args_str      = string.substr(paren_open + 1, paren_close - paren_open - 1);

	u64               start = 0;
	OwnedArgumentList arguments;
	while (start < args_str.length()) {
		u64 end = args_str.find(',', start);

		// Last argument.
		if (end == std::string::npos) end = args_str.length();

		std::string arg = strip(args_str.substr(start, end - start));
		try {
			if (!arg.empty()) arguments.push_back(getIntVmValue(pid, std::stoll(arg)));
		} catch (const std::exception& e) {
			std::cerr << "Error: Invalid argument '" << arg << "' - must be integer\n";
			throw -1;
		}
		start = end + 1;
	}
	if (!vm::api::runFunction(pid, function_name, createArgumentList(arguments)))
		throw -1;
	get_exit_value();
	freeArguments(arguments);
}

void DuckVMDebug::get_exit_value(){
	if (!vm::api::join(pid).has_value()) throw -1;

	auto exit_code_response = vm::api::getExitValue(pid);
	if (!exit_code_response.has_value()) throw -1;

	if (exit_code_response.value()->type->getName() != base::StrID("i64"))
		throw -1;
	std::cout<< "Ret: " << exit_code_response.value()->readBytes<i64>() << "\n";
}

void DuckVMDebug::get_status(){
	auto response = vm::api::getExecutionStatus(pid);

	//TODO: Process response and print status
}

void DuckVMDebug::step(){
	if (!vm::api::step(pid)) throw -1;
	// auto response = vm::api::getCurrentPosition(pid);
	// if (!response.has_value()) throw -1;
	// std::cout << response.value().function_id << " " << response.value().instr_number << "\n";
}

void DuckVMDebug::resume(){
	if (!vm::api::resume(pid)) throw -1;
}

void DuckVMDebug::pause(){
	if (!vm::api::pause(pid)) throw -1;
}