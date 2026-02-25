#pragma once

#include "vm/api/data/status.hpp"
#include <vm/api/vm.hpp>
#include <vm/vm_debug_core.hpp>

#include <ostream>

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
	enum class Mode { Run, Command };
	Mode                   mode = Mode::Command;
	static DuckVMDebugCli* active_instance;
	std::stringstream      vm_input_stream;
	std::ostream*          vm_output_stream;
	DuckVMDebugCore        core;
	int                    event_fd;
	int                    signal_pipe[2];
	int                    output_pipe[2];

	DuckVMDebugCli();
	DuckVMDebugCli(const fs::File& filepath, const std::vector<std::string>& args = {});

	// returns if cli should be still running
	bool        handleLine(std::string& line);
	void        handleEvent();
	void        help() const;
	void        changeMode(vm::api::ProcStatus& status);
	Mode        modeFromStatus(vm::api::ProcStatus& status);
	void        handleTstp(int signo);
	static void staticHandleTstp(int signo);
	void        setSigaction(Mode& target);
};
