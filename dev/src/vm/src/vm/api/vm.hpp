/**
 * @file vm.hpp
 * @brief Main API for the VM clients (CLI, Server, etc.)
 */
#pragma once

#include <filesystem/file.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/response.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/thread/vmvalue.hpp>

#include <vector>

namespace vm::api {
	/**
	 * @brief Create new process in the api
	 */
	std::expected<ProcessInfo, ApiError> spawn();

	/**
	 * @brief Deinitializes and validates process'es memory state.
	 * It also removes the process from the internal structures.
	 * @TODO: #1354 After 1354 it should be required that the process is stopped/finished
	 * when this endpoint is called.
	 */
	std::expected<response::Boolean, ApiError> deinitAndValidate(PID pid);

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

	std::expected<void, ApiError> loadFiles(PID pid, const std::vector<fs::File>& path);
	std::expected<void, ApiError> loadCode(PID pid, const code::CodeCollection& code);
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

	/**
	 * @brief Returns a VmValue containing the return value of the last ran function on DVM.
	 * @note The returned VmValue is owned by the process and shouldn't be freed by the caller. It
	 * will be automatically freed when the process is destructed.
	 * @return The VmValue containing the return value of the last called function.
	 */
	std::expected<ExitValue, ApiError> getExitValue(PID pid);

	std::expected<response::Output, ApiError> output(PID pid);

	std::expected<response::Type, ApiError> getType(PID pid, const std::string& type_name);


	/**
	 * @brief Returns an empty VmValue (initialized by zero bytes) of the given type.
	 * @note This endpoint returns a VmValue which is owned by the caller. It's the callers
	 * responsibility to call `VmValue::freeData()` on the VmValue.
	 * @return Response containing a Box containing the newly allocated VmValue of the specified type.
	 */
	std::expected<response::VmValue, ApiError> getVmValue(PID pid, const std::string& type_name);
	std::expected<response::CodePosition, ApiError> getCurrentPosition(PID pid);
}
