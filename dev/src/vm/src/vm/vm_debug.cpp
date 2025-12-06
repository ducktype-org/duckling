#include "vm_debug.hpp"

#include <iostream>
#include <string>


namespace {
	std::string strip(std::string string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
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
		} else if (stripped_line == "run") {
			run_vm();
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