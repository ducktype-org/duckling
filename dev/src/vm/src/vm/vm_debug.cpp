#include "vm_debug.hpp"

#include <iostream>
#include <string>


namespace {
	// Box<vm::VmValue> getIntVmValue(vm::PID pid, i64 value) {
	// 	auto response = vm::api::getVmValue(pid, "i64");
	// 	if (!response.has_value()) throw ReplFailedToCreateAVmValue();
	// 	auto vm_value = std::move(response->vm_value);
	// 	vm_value->writeBytes<i64>(value);
	// 	return vm_value;
	// }

	std::string strip(std::string string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
	}

	std::string lstrip(std::string string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		return string;
	}

	// /**
	//  * @brief Create a list of references to owned VmValues which can be passed to the VM.
	//  */
	// vm::FunctionRunArguments createArgumentList(DuckVMRepl::OwnedArgumentList& arguments) {
	// 	return arguments | std::views::transform([](auto& value) { return value.refMut(); })
	// 	     | std::ranges::to<vm::FunctionRunArguments>();
	// }

	// void freeArguments(DuckVMRepl::OwnedArgumentList& arguments) {
	// 	for (auto& arg: arguments) arg->freeData();
	// }
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
		} else if (stripped_line == "resume") {
			resume();
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

	if (!vm::api::run(pid, debug_args)) throw -1;
	if (!vm::api::join(pid)) throw -1;
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