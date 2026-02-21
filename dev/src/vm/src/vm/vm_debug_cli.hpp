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
	DuckVMDebugCore core;

	DuckVMDebugCli();
	DuckVMDebugCli(const fs::File& filepath, const std::vector<std::string>& args = {});

	// returns if cli should be still running
	bool handleLine(std::string line);
	void handleEvent();
	void help() const;
};
