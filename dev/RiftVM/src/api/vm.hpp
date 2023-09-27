#pragma once

#include "services/service_manager.hpp"
#include "services_data/data_manager.hpp"
#include <api/api.hpp>
#include <filesystem/file.hpp>

namespace vm {
	// @TODO: move somewhere else
	using DataManager    = DataManagerDef<>;
	using ServiceManager = ServiceManagerDef<ReferenceCounter, Profiler>;
}

namespace vm::api {
	/**
	 * @brief Create new process in the api
	 * @return
	 */
	result<ProcessInfo, ApiError> spawn(bool usesStdio);

	/**
	 * @brief Get the execution status of the RiftVM
	 * @return
	 */
	result<VCPUStatus, ApiError> getExecutionStatus(PID pid);

	/**
	 * @brief Pauses the execution of the program.
	 * The program will be paused at the next nearest safe point.
	 * When this function returns running, the program is paused. If false, the state is undefined.
	 * @return
	 */
	result<void, ApiError> pause(PID pid);
	/** @brief Resumes the execution of the program.
	 * When this function returns true, the program is running. If false, the state is undefined.
	 * @return
	 */
	result<void, ApiError> resume(PID pid);
	result<void, ApiError> step(PID pid);

	result<void, ApiError>             loadFile(PID pid, const fs::FilePath &path);
	result<void, ApiError>             run(PID pid);
	result<void, ApiError>             join(PID pid);
	result<void, ApiError>             stop(PID pid);
	result<void, ApiError>             kill(PID pid);
	result<void, ApiError>             input(PID pid, const std::string &input);
	result<response::Output, ApiError> output(PID pid);

	result<TypeCRef, ApiError>        getType(PID pid, const std::string &type_name);
	result<response::Block, ApiError> getBlock(PID pid, u64 block_id);
}
