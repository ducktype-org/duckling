/**
 * @file vm.hpp
 * @brief Main API for the VM clients (CLI, Server, etc.)
 */
#pragma once

#include <api/api.hpp>
#include <filesystem/file.hpp>

namespace vm::api {
	/**
	 * @brief Create new process in the api
	 */
	cpp::result<ProcessInfo, ApiError> spawn();

	/**
	 * @brief Get the execution status of the VM
	 */
	cpp::result<ProcStatus, ApiError> getExecutionStatus(PID pid);

	/**
	 * @brief Pauses the execution of the program.
	 * The program will be paused at the next nearest safe point.
	 * When this function returns running, the program is paused. If false, the state is undefined.
	 * @return
	 */
	cpp::result<void, ApiError> pause(PID pid);
	/** @brief Resumes the execution of the program.
	 * When this function returns true, the program is running. If false, the state is undefined.
	 * @return
	 */
	cpp::result<void, ApiError> resume(PID pid);
	cpp::result<void, ApiError> step(PID pid);

	cpp::result<void, ApiError> loadFile(PID pid, const fs::FilePath& path);
	cpp::result<void, ApiError> run(PID pid);
	cpp::result<void, ApiError> join(PID pid);
	cpp::result<void, ApiError> attach(PID pid, std::istream& input, std::ostream& output);
	cpp::result<void, ApiError> detach(PID pid);
	cpp::result<void, ApiError> stop(PID pid);
	cpp::result<void, ApiError> kill(PID pid);
	cpp::result<void, ApiError> input(PID pid, const std::string& input);
	cpp::result<response::Output, ApiError> output(PID pid);

	cpp::result<TypeCRef, ApiError>        getType(PID pid, const std::string& type_name);
	cpp::result<response::Block, ApiError> getBlock(PID pid, u64 block_id);
	// cpp::result<response::BlockIDs, ApiError> getBlocks(PID pid);
	cpp::result<response::CodePosition, ApiError> getCurrentPosition(PID pid);
}
