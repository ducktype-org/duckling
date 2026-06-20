/**
 * @file vm.hpp
 * @brief Main API for the VM clients (CLI, Server, etc.)
 */
#pragma once


#include <vm/api/api.hpp>
#include <vm/api/data/response.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/vmvalue/vmvalue.hpp>

namespace vm::api {
	/**
	 * @brief Create new process in DVM.
	 * @param enable_deadlock_detection When true (defaults to false), the process will detect
	 * circular mutex wait chains at runtime and throw VMDeadlockException. When false, deadlock
	 * detection is skipped and circular waits will block indefinitely until the process is stopped.
	 * @return The response containing the PID of the newly created process or an API error if the
	 * process wasn't created.
	 */
	std::expected<ProcessInfo, ApiError> spawn(bool enable_deadlock_detection = false);

	/**
	 * @brief Get the execution status of the process run on DVM.
	 * @return The execution status of the specified process or an API error.
	 */
	std::expected<ProcStatus, ApiError> getExecutionStatus(PID pid);

	/**
	 * @brief Load the code from given files into a specified process on DVM.
	 * @note This endpoint can be called multiple times and used to load code in separate requests.
	 * @return Nothing if the code was loaded successfully or an API error otherwise (ex. syntax
	 * errors, static verification errors, duplicate function errors).
	 */
	std::expected<void, ApiError> loadFiles(PID pid, const std::vector<fs::File>& path);

	/**
	 * @brief Load the code from given code collection into a specified process on DVM.
	 * @note This endpoint can be called multiple times and used to load code in separate requests.
	 * @return Nothing if the code was loaded successfully or an API error otherwise (ex. syntax
	 * errors, static verification errors, duplicate function errors).
	 */
	std::expected<void, ApiError> loadCode(PID pid, const code::CodeCollection& code);

	/**
	 * @brief Run a program on DVM. It is expected that a 'main' function was loaded into the
	 * process before this endpoint is called.
	 * @note The exit value of the program can be retrieved by the `getExitValue` endpoint.
	 *
	 * @return Nothing if the program was run successfully or an API error otherwise.
	 */
	std::expected<void, ApiError> run(PID pid, const ProgramRunArguments& args = {});

	/**
	 * @brief Run a function with a given name on DVM.
	 * @note The exit value of the called function can be retrieved by the `getExitValue` endpoint.
	 *
	 * @return ThreadID if the function was run successfully or an API error otherwise.
	 */
	std::expected<ThreadID, ApiError> runFunction(
		PID pid, const std::string& func_name, const FunctionRunArguments& args = {}
	);

	/**
	 * @brief Same as runFunction, but executes the function synchronously on the caller's thread and
	 * returns it's return value.
	 * @return The return value of the function if it was ran successfully or an API error otherwise.
	 */
	std::expected<ExitValue, ApiError> runFunctionAwait(
		PID pid, const std::string& func_name, const FunctionRunArguments& args = {}
	);

	/**
	 * @brief Get a VmValue containing the return value of the last ran function on DVM.
	 * @note The returned VmValue is owned by the process and shouldn't be freed by the caller. It
	 * will be automatically freed when the process is destroyed.
	 * @return The VmValue containing the return value of the last called function or an API error
	 * if no function was run or the execution didn't complete yet.
	 */
	std::expected<ExitValue, ApiError> getExitValue(PID pid);

	/**
	 * @brief Wait for the execution thread of the given process to stop.
	 * @return Nothing if the thread successfully stopped or an API error otherwise, in which case
	 * the state is undefined.
	 */
	std::expected<void, ApiError> join(PID pid, ThreadID thread_id);
	std::expected<void, ApiError> join(PID pid);

	/**
	 * @brief Request the main execution thread of the given process to stop running, kill the
	 * execution thread and kill the process.
	 * @return Nothing if the process was successfully killed or an API error otherwise.
	 */
	std::expected<void, ApiError> kill(PID pid);

	/**
	 * @brief Deinitialize and validate processes memory state.
	 * Also, remove the process from the internal structures.
	 * @TODO: #1354 After 1354 it should be required that the process is stopped/finished
	 * when this endpoint is called.
	 */
	std::expected<response::Boolean, ApiError> deinitAndValidate(PID pid);

	/// DEBUGGER REQUESTS ///
	/**
	 * @brief Pause the execution of the program.
	 * The program will be paused at the next nearest safe point and can be resumed by using the
	 * `resume` endpoint.
	 * @return Code position of the next instruction to execute after the program is paused or an
	 * error in which case the state is undefined.
	 */
	std::expected<response::CodePosition, ApiError> pause(PID pid, ThreadID thread_id);
	std::expected<response::CodePosition, ApiError> pause(PID pid);

	/**
	 * @brief Resume the execution of the program.
	 * @return Nothing if the program successfully resumed or an API error otherwise, in which case
	 * the state is undefined.
	 */
	std::expected<void, ApiError> resume(PID pid, ThreadID thread_id);
	std::expected<void, ApiError> resume(PID pid);

	/**
	 * @brief Perform one instruction of the program and pause.
	 * @return Nothing if the program successfully stepped and paused or an API error otherwise, in
	 * which case the state is undefined.
	 */
	std::expected<void, ApiError> step(PID pid);

	/**
	 * @brief Force the main execution thread of the given process to stop running and kill the
	 * execution thread. Don't kill the process itself.
	 * @return Nothing if the thread successfully stopped or an API error otherwise, in which case
	 * the state is undefined.
	 */
	std::expected<void, ApiError> stop(PID pid);

	/**
	 * @brief Wait for breakpoint hit. Used by tests.
	 */
	std::expected<response::CodePosition, ApiError> waitForBreakpoint(PID pid);

	/**
	 * @brief Get the code position of the next line of bytecode to be executed on the specified
	 * process of the DVM.
	 * @return The response containing code position or an API error.
	 */
	std::expected<response::CodePosition, ApiError> getCurrentPosition(PID pid);

	/// IO REQUESTS ///
	/**
	 * @brief Attach an input and an output stream to a process with a given ID.
	 * @return Nothing if the streams attached successfully or an API error otherwise.
	 */
	std::expected<void, ApiError> attach(PID pid, std::istream& input, std::ostream& output);

	/**
	 * @brief Detach the input and output streams from the process with a given ID.
	 * @return Nothing if the streams were detached successfully or an API error otherwise.
	 */
	std::expected<void, ApiError> detach(PID pid);

	/**
	 * @brief Pass an input string to a program run on the specified process.
	 * @return Nothing if the input was passed successfully or an API error otherwise.
	 */
	std::expected<void, ApiError> input(PID pid, const std::string& input);

	/**
	 * @brief Request an output string from a program ran on the specified process.
	 * @return The response containing a string with the program output or an API error otherwise.
	 */
	std::expected<response::Output, ApiError> output(PID pid);

	/// DATA REQUESTS ///
	/**
	 * @brief Get a DVM Type representation of the type specified by the name.
	 * @return The response containing a constant 'Type' reference or an API error.
	 */
	std::expected<response::Type, ApiError> getType(PID pid, const std::string& type_name);

	/**
	 * @brief Get an empty VmValue (initialized by zero bytes) of the given type.
	 * @note This endpoint returns a VmValue which is owned by the caller. It's the callers
	 * responsibility to call `VmValue::freeData()` on the VmValue. For more information
	 * on why this is necessary, see documentation of `vm::VmValue::freeData()`.
	 * @return Response containing a Box containing the newly allocated VmValue of the specified type.
	 */
	std::expected<response::VmValue, ApiError> getVmValue(PID pid, const std::string& type_name);

	/**
	 * @brief Get the number of current stack frames.
	 * @return The response containing the number of stack frames or an API error.
	 */
	std::expected<response::NumberOfCurrentStackFrames, ApiError> debuggerGetNumberOfStackFrames(
		PID pid, ThreadID thread_id
	);

	/**
	 * @brief Get the variables of a stack frame with a given index.
	 * @return The response containing the variables of the stack frame or an API error.
	 */
	std::expected<response::StackFrameData, ApiError> debuggerGetStackFrameData(
		PID pid, ThreadID thread_id, u64 stack_frame_number
	);

	/**
	 * @brief Attaches Listener to the on_status_changed Emitter
	 * @return Nothing if attached successfully
	 */
	std::expected<void, ApiError> attachStatusListener(
		PID pid, Ref<events::Listener<ProcStatus>> listener
	);

	/**
	 * @brief Attaches Listener to the output emitter
	 * @return Nothing if attached successfully
	 */
	std::expected<void, ApiError> attachOutputListener(
		PID pid, Ref<events::Listener<std::string>> listener
	);

	/**
	 * @brief Enables or disables breakpoint on a given instruction in a given function.
	 * @return Nothing if the breakpoint was set successfully or an API error otherwise.
	 * @note Enabling a breakpoint on an instruction that already has a breakpoint or disabling a
	 * breakpoint on an instruction that doesn't have a breakpoint is considered successful and
	 * doesn't return an error.
	 */
	std::expected<void, ApiError> setBreakpoint(
		PID pid, base::StrID function_name, u64 instruction_index, bool enable
	);

	/**
	 * @brief Gets the first code collection instruction that starts in the provided file line.
	 * @return Either the mapped `CodePosition` on success, or a nullopt if no such instruction
	 * exists.
	 */
	std::expected<response::CodePosition, ApiError> mapFileLineToCodeCollectionPosition(
		PID pid, fs::File file, usize line_number
	);
}
