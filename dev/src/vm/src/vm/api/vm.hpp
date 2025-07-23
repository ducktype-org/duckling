/**
 * @file vm.hpp
 * @brief Main API for the VM clients (CLI, Server, etc.)
 */
#pragma once

#include <filesystem/file.hpp>

#include "vm/api/data/response.hpp"
#include "vm/core/thread/vmvalue.hpp"
#include <vm/api/api.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/interface_types.hpp>

#include <vector>

namespace vm::api {
	/**
	 * @brief Create new process in the api
	 */
	std::expected<ProcessInfo, ApiError> spawn();

	/**
	 * @brief Get the execution status of the VM
	 */
	std::expected<ProcStatus, ApiError> getExecutionStatus(PID pid);

	/**
	 * @brief Pauses the execution of the program.
	 * The program will be paused at the next nearest safe point.
	 * When this function returns running, the program is paused. If false, the state is undefined.
	 * @return
	 */
	std::expected<response::CodePosition, ApiError> pause(PID pid);
	/** @brief Resumes the execution of the program.
	 * When this function returns true, the program is running. If false, the state is undefined.
	 * @return
	 */
	std::expected<void, ApiError> resume(PID pid);
	std::expected<void, ApiError> step(PID pid);
	/**
	 * @brief Wait for breakpoint hit. Used by tests.
	 */
	std::expected<response::CodePosition, ApiError> waitForBreakpoint(PID pid);

	std::expected<void, ApiError> loadStdlib(PID pid);
	std::expected<void, ApiError> loadFiles(PID pid, const std::vector<fs::File>& path);
	std::expected<void, ApiError> loadCode(PID pid, const std::vector<code::CodeCollection>& code);
	std::expected<void, ApiError> run(PID pid, const ProgramRunArguments& args = {});
	std::expected<void, ApiError> runFunction(
		PID pid, const std::string& func_name, const FunctionRunArguments& args = {}
	);
	std::expected<void, ApiError> join(PID pid);
	std::expected<void, ApiError> attach(PID pid, std::istream& input, std::ostream& output);
	std::expected<void, ApiError> detach(PID pid);
	std::expected<void, ApiError> stop(PID pid);
	std::expected<void, ApiError> kill(PID pid);
	std::expected<void, ApiError> input(PID pid, const std::string& input);

	std::expected<CRef<VmValue>, ApiError> getExitValue(PID pid);

	std::expected<response::Output, ApiError> output(PID pid);

	std::expected<TypeCRef, ApiError>        getType(PID pid, const std::string& type_name);
	std::expected<response::Block, ApiError> getBlock(PID pid, u64 block_id);

	/**
	 * @brief Returns an empty VmValue (initialized by zero) of the given type.
	 */
	std::expected<response::VmValue, ApiError> getVmValue(PID pid, const std::string& type_name);
	std::expected<response::CodePosition, ApiError> getCurrentPosition(PID pid);
}
