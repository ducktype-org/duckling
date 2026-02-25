#include "vm_debug_cli.hpp"

#include <ext/stdio_filebuf.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/status.hpp>
#include <vm/vm_debug_core.hpp>

#include <csignal>
#include <iostream>
#include <string>
#include <variant>

DuckVMDebugCli* DuckVMDebugCli::active_instance = nullptr;

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

	struct pollfd fds[4] = { { .fd = STDIN_FILENO, .events = POLLIN, .revents = 0 },
		                     // vm state(status) change
		                     { .fd = event_fd, .events = POLLIN, .revents = 0 },
		                     // ctrl-Z handling
		                     { .fd = signal_pipe[0], .events = POLLIN, .revents = 0 },
		                     // vm output
		                     { .fd = output_pipe[0], .events = POLLIN, .revents = 0 } };


	while (true) {
		if (mode == Mode::Command) {
			std::cout << "\n>>> ";
			std::cout.flush();
		}
		int ret = poll(fds, std::size(fds), -1);
		if (ret < 0) {
			if (errno == EINTR) continue;
			throw std::runtime_error("select() failed");
		}
		// VM event arrived
		if (fds[1].revents & POLLIN) {
			uint8_t byte;
			read(event_fd, &byte, 1);
			handleEvent();
		}  // User typed something
		if (mode == Mode::Command && fds[0].revents & POLLIN) {
			if (!std::getline(std::cin, line)) {
				std::cout << "Exiting debugger\n";
				break;
			}
			if (!handleLine(line)) break;
		}
		if (mode == Mode::Run && fds[0].revents & POLLIN) {
			// TODO: handle input of vm
		}
		if (fds[2].revents & POLLIN) {
			uint8_t b;
			read(signal_pipe[0], &b, 1);
			core.pause();
		}
		if (fds[3].revents & POLLIN) {
			char buf[256];
			std::cout << "\nVM wrote:\n";
			while (true) {
				ssize_t n = read(output_pipe[0], buf, sizeof(buf));
				if (n > 0)
					std::cout << std::string(buf, static_cast<std::size_t>(n));
				else
					break;
			}
			std::cout << "\n\n";
		}
	}
}

void DuckVMDebugCli::handleEvent() {
	std::cout << "VM says:\n";
	auto status = core.getStatus();
	changeMode(status);
}

void DuckVMDebugCli::changeMode(vm::api::ProcStatus& status) {
	auto target = modeFromStatus(status);
	if (mode != target) setSigaction(mode = target);
}

DuckVMDebugCli::Mode DuckVMDebugCli::modeFromStatus(vm::api::ProcStatus& status) {
	if (std::holds_alternative<vm::api::Running>(status)
	    || std::holds_alternative<vm::api::WaitingForInput>(status))
		return Mode::Run;
	else
		return Mode::Command;
}

bool DuckVMDebugCli::handleLine(std::string& line) {
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

void DuckVMDebugCli::setSigaction(Mode& target) {
	struct sigaction custom_tstp;
	sigemptyset(&custom_tstp.sa_mask);
	if (target == Mode::Command)
		custom_tstp.sa_handler = SIG_DFL;
	else
		custom_tstp.sa_handler = staticHandleTstp;
	custom_tstp.sa_flags = 0;

	sigaction(SIGTSTP, &custom_tstp, nullptr);
}

void DuckVMDebugCli::handleTstp(int signo) {
	uint8_t b = 1;
	write(signal_pipe[1], &b, 1);
}

void DuckVMDebugCli::staticHandleTstp(int signo) {
	if (active_instance) active_instance->handleTstp(signo);
}

DuckVMDebugCli DuckVMDebugCli::get(const fs::File& filepath, const std::vector<std::string>& args) {
	return { filepath, args };
}

DuckVMDebugCli::DuckVMDebugCli(): vm_input_stream(), core(DuckVMDebugCore(std::cin, std::cout)) {
	active_instance = this;

	if (pipe(signal_pipe) < 0) throw std::runtime_error("Failed to create signal pipe");

	fcntl(signal_pipe[0], F_SETFL, O_NONBLOCK);
	fcntl(signal_pipe[1], F_SETFL, O_NONBLOCK);
}

DuckVMDebugCli::DuckVMDebugCli(const fs::File& filepath, const std::vector<std::string>& args) {
	active_instance = this;

	if (pipe(signal_pipe) < 0) throw std::runtime_error("Failed to create signal pipe");

	fcntl(signal_pipe[0], F_SETFL, O_NONBLOCK);
	fcntl(signal_pipe[1], F_SETFL, O_NONBLOCK);

	if (pipe(output_pipe) < 0) throw std::runtime_error("Failed to create signal pipe");

	fcntl(output_pipe[0], F_SETFL, O_NONBLOCK);
	fcntl(output_pipe[1], F_SETFL, O_NONBLOCK);

	auto* file_buf = new __gnu_cxx::stdio_filebuf<char>(output_pipe[1], std::ios::out);

	vm_output_stream = (new std::ostream(file_buf));

	core = DuckVMDebugCore(filepath, std::cin, *vm_output_stream, args);
}

void DuckVMDebugCli::help() const {
	std::cout << "BeRD Debugger Commands:\n"
				 "  s, step                     - Execute one step in the VM\n"
				 "  run                         - Run the VM until completion\n"
				 "  run <func([args])>          - Run a specific function with arguments\n"
				 "  resume, continue, c         - Resume execution of the VM\n"
				 "  pause                       - Pause execution of the VM\n"
				 "  cp, pos, position           - Show current position\n"
				 "  exit, q, quit               - Exit the debugger\n"
				 "  help, h, ?                  - Show this help message\n";
}
