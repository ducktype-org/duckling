#pragma once

#include "vm/vm_debug_core.hpp"
#include <vm/api/vm.hpp>

/**
 * @file vm_debug_cli.hpp
 * @brief Command line interface for the VM debugger.
 */


class DuckVMDebugCli {
public:
	using OwnedArgumentList = std::vector<Box<vm::VmValue>>;

	static DuckVMDebugCli get(const fs::File& filepath, const std::vector<std::string>& args = {});

	DuckVMDebugCli(DuckVMDebugCli&&)                 = delete;
	DuckVMDebugCli& operator=(DuckVMDebugCli&&)      = delete;
	DuckVMDebugCli(const DuckVMDebugCli&)            = delete;
	DuckVMDebugCli& operator=(const DuckVMDebugCli&) = delete;

	void run();

private:
	static DuckVMDebugCli* active_instance;
	std::stringstream vm_input_stream;
	std::stringstream vm_output_stream;
	DuckVMDebugCore core;
	int event_fd;
	int signal_pipe[2];

	DuckVMDebugCli();
	DuckVMDebugCli(const fs::File& filepath, const std::vector<std::string>& args = {});

	// returns if cli should be still running
	bool handleLine(std::string& line);
	// returns true if debugger cli should be in runmode
	bool handleEvent();
	void help() const;
	void enterCommandMode();
	void enterRunMode();
	void handleTstp(int signo);
	static void staticHandleTstp(int signo);
	void setSigaction(bool to_normal);
};
