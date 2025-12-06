#pragma once

#include <vm/api/vm.hpp>

/**
 * @file vm_debug.hpp
 * @brief Command line interface for the VM debugger.
 */


class DuckVMDebug {
public:
	using OwnedArgumentList = std::vector<Box<vm::VmValue>>;

	static DuckVMDebug get(const fs::File& filepath, const std::vector<std::string>& args = {});

	DuckVMDebug(DuckVMDebug&&)                 = delete;
	DuckVMDebug& operator=(DuckVMDebug&&)      = delete;
	DuckVMDebug(const DuckVMDebug&)            = delete;
	DuckVMDebug& operator=(const DuckVMDebug&) = delete;

	void run();

private:
	struct CallInfo {
		std::string       func_name;
		OwnedArgumentList func_args;
	};

	vm::PID pid{};
	u64     step_counter = 0;

	std::vector<std::string> debug_args;

	DuckVMDebug();
	DuckVMDebug(const fs::File& filepath, const std::vector<std::string>& args = {});

	void run_vm();
};