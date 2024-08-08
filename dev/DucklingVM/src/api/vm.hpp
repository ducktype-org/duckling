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
	cpp::result<ProcessInfo, ApiError> spawn(bool usesStdio);

	/**
	 * @brief Get the execution status of the DucklingVM
	 * @return
	 */
	cpp::result<VCPUStatus, ApiError> getExecutionStatus(PID pid);

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

	cpp::result<void, ApiError>             loadFile(PID pid, const fs::FilePath& path);
	cpp::result<void, ApiError>             run(PID pid);
	cpp::result<void, ApiError>             join(PID pid);
	cpp::result<void, ApiError>             stop(PID pid);
	cpp::result<void, ApiError>             kill(PID pid);
	cpp::result<void, ApiError>             input(PID pid, const std::string& input);
	cpp::result<response::Output, ApiError> output(PID pid);

	cpp::result<TypeCRef, ApiError>        getType(PID pid, const std::string& type_name);
	cpp::result<response::Block, ApiError> getBlock(PID pid, u64 block_id);
}
