#include "vm_debug_cli.hpp"

#include "vm/api/data/api_error.hpp"
#include "vm/api/data/status.hpp"
#include "vm/vm_debug_core.hpp"

#include <iostream>
#include <string>
#include <variant>

namespace {

	std::string strip(std::string string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
	}

	std::string lstrip(std::string string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		return string;
	}
}

void DuckVMDebugCli::run() {
	std::cout << "++++++++++++++++++++++++\n"
				 "+   BeRD has started   +\n"
				 "++++++++++++++++++++++++\n";

	std::string line;
	int         event_fd = core.getEventPipeReadFD();
	while (true) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		FD_SET(event_fd, &fds);
		int maxfd = std::max(STDIN_FILENO, event_fd);
		std::cout << "\n>>> ";
		std::cout.flush();
		int ret = select(maxfd + 1, &fds, nullptr, nullptr, nullptr);
		if (ret < 0) {
			if (errno == EINTR) continue;
			throw std::runtime_error("select() failed");
		}
		// VM event arrived
		if (FD_ISSET(event_fd, &fds)) {
			uint8_t byte;
			read(event_fd, &byte, 1);
			handleEvent();
			continue;
		}  // User typed something
		if (FD_ISSET(STDIN_FILENO, &fds)) {
			if (!std::getline(std::cin, line)) {
				std::cout << "Exiting debugger\n";
				break;
			}
			if (!handleLine(line)) break;
		}
	}
}

void DuckVMDebugCli::handleEvent() {
	std::cout << "VM says:\n";
	core.getStatus();
}

bool DuckVMDebugCli::handleLine(std::string line) {
	std::string stripped_line = strip(line);
	if (line.empty()) return true;
	if (stripped_line == "exit" || stripped_line == "q" || stripped_line == "quit") {
		std::cout << "Exiting BeRD\n";
		return false;
	}
	if (stripped_line == "s" || stripped_line == "step")
		core.step();
	else if (stripped_line == "run")
		core.runVm();
	else if (stripped_line.starts_with("run "))
		core.runFun(lstrip(lstrip(line).substr(3)));
	else if (stripped_line == "resume" || stripped_line == "continue" || stripped_line == "c")
		core.resume();
	else if (stripped_line == "pause")
		core.pause();
	else if (stripped_line == "stop")
		core.stop();
	else if (stripped_line == "cp" || stripped_line == "pos" || stripped_line == "position")
		core.getCurrentPosition();
	else if (stripped_line == "help" || stripped_line == "?" || stripped_line == "h")
		help();
	else if (stripped_line == "mem" || stripped_line == "m")
		core.printMemory();
	else
		std::cout << "Invalid input: \"" << line << "\"\n";
	return true;
}

DuckVMDebugCli DuckVMDebugCli::get(const fs::File& filepath, const std::vector<std::string>& args) {
	return { filepath, args };
}

DuckVMDebugCli::DuckVMDebugCli(): core(DuckVMDebugCore()) {}

DuckVMDebugCli::DuckVMDebugCli(const fs::File& filepath, const std::vector<std::string>& args):
	  core(DuckVMDebugCore(filepath, args)) {}

void DuckVMDebugCli::help() const {
	std::cout << "BeRD Debugger Commands:\n"
				 "  s, step                     - Execute one step in the VM\n"
				 "  run                         - Run the VM until completion\n"
				 "  run <func([args])>          - Run a specific function with arguments\n"
				 "  resume, continue, c	        - Resume execution of the VM\n"
				 "  pause                       - Pause execution of the VM\n"
				 "  cp, pos, position           - Show current position\n"
				 "  exit, q, quit               - Exit the debugger\n"
				 "  help, h, ?                  - Show this help message\n"
				 "  mem, m                      - Show program memory\n";
}
