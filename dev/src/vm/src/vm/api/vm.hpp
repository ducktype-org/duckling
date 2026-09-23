/**
 * @file vm.hpp
 * @brief Main API for the VM clients (CLI, Server, etc.)
 */
#pragma once


#include <vm/api/api.hpp>
#include <vm/api/data/execution_config.hpp>
#include <vm/api/data/process_options.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/thread_id.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>

namespace vm::api {
	/**
	 * @brief Create new process in DVM.
	 * @param options Process configuration. `options.mode` selects the execution mode (Safe/Fast);
	 * `options.enable_deadlock_detection`, when true (defaults to false), makes the process detect
	 * circular mutex wait chains at runtime and throw VMDeadlockException. When false, deadlock
	 * detection is skipped and circular waits will block indefinitely until the process is stopped.
	 * @return The response containing the PID of the newly created process or an API error if the
	 * process wasn't created.
	 */
	std::expected<ProcessInfo, ApiError> spawn(const ProcessConfig& options = {});

	/**
	 * @brief Get the execution status of the process run on DVM.
	 * @return The execution status of the specified process or an API error.
	 */
	std::expected<ProcStatus, ApiError> getExecutionStatus(PID pid);

	/**
	 * @brief Set the execution config of a VMProcess.
	 * @note This affects static checks performed when *loading new code*. While data computed for
	 * already loaded code does not need to be recomputed, compliance of old code wrt. the config
	 * is *not* checked.
	 * @return Nothing if the config was set successfully or an API error otherwise.
	 */
	std::expected<void, ApiError> setExecutionConfig(PID pid, ExecutionConfig config);

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
	 * @brief Same as `run`, but executes the program synchronously on the caller's thread and
	 * returns its exit value.
	 * @return The return value of the program if it was ran successfully or an API error otherwise.
	 */
	std::expected<ExitValue, ApiError> runAwait(PID pid, const ProgramRunArguments& args = {});

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
	 * @brief Get a VMValue containing the return value of the last ran function on DVM.
	 * @note The returned VMValue is owned by the process and shouldn't be freed by the caller. It
	 * will be automatically freed when the process is destroyed.
	 * @return The VMValue containing the return value of the last called function or an API error
	 * if no function was run or the execution didn't complete yet.
	 */
	std::expected<ExitValue, ApiError> getExitValue(PID pid);

	/**
	 * @brief Wait for the execution thread of the given process to stop.
	 * @return Nothing if the thread successfully stopped or an API error otherwise, in which case
	 * the state is undefined.
	 */
	std::expected<void, ApiError> join(PID pid, ThreadID thread_id = api::MAIN_THREAD_ID);

	/**
	 * @brief Request the main execution thread of the given process to stop running, kill the
	 * execution thread and kill the process.
	 * @return Nothing if the process was successfully killed or an API error otherwise.
	 */
	std::expected<void, ApiError> kill(PID pid);

	/**
	 * @brief Deinitialize and validate processes memory state.
	 *
	 * @note Legal only when a process never started or completed successfully without being stopped
	 * or panicked. Still executing process or one that was stopped or panicked will be refused with
	 * a `StateError`. Such process should be killed.
	 *
	 * @note The process is removed from the internal structures whenever the deinitialization
	 * actually ran. This includes a successful deinit and when a global destructor panicked during
	 * execution.
	 */
	std::expected<response::Boolean, ApiError> deinitAndValidate(PID pid);

	/**
	 * @brief Tears down the process. If it's possible, we tear it down gracefully with
	 * `deinitAndValidate`, otherwise (still running, stopped or panicked) we force `kill` it. In
	 * both cases, the process is gone.
	 *
	 * @return The validation result if the process was deinitialized, an empty optional if it had
	 * to be killed, or an API error if neither could be done.
	 */
	std::expected<base::Optional<response::Boolean>, ApiError> deinitOrKill(PID pid);

	/// DEBUGGER REQUESTS ///
	/**
	 * @brief Pause the execution of the program.
	 * The program will be paused at the next nearest safe point and can be resumed by using the
	 * `resume` endpoint.
	 * @return Code position of the next instruction to execute after the program is paused or an
	 * error in which case the state is undefined.
	 */
	std::expected<response::CodePosition, ApiError> pause(
		PID pid, ThreadID thread_id = api::MAIN_THREAD_ID
	);

	/**
	 * @brief Asks every running thread to pause and waits for them to be paused.
	 *
	 * Blocks until every asked thread parked. A thread sleeping on IO pauses once it wakes up
	 * from the IO.
	 *
	 * A thread is skipped when another control request is still in progress, or when it
	 * finishes first. Skipped threads are not included in the result.
	 *
	 * @return The threads that are actually paused.
	 */
	std::expected<response::ThreadIDs, ApiError> pauseAll(PID pid);

	/**
	 * @brief Resume the execution of the program.
	 * @return Nothing if the program successfully resumed or an API error otherwise, in which case
	 * the state is undefined.
	 */
	std::expected<void, ApiError> resume(PID pid, ThreadID thread_id = api::MAIN_THREAD_ID);

	/**
	 * @brief Perform one instruction of the given (paused) thread and pause again.
	 *
	 * @return Nothing if the thread successfully stepped (and paused again or terminated) or an API
	 * error otherwise.
	 */
	std::expected<void, ApiError> step(PID pid, ThreadID thread_id = api::MAIN_THREAD_ID);

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
	std::expected<response::CodePosition, ApiError> waitForBreakpoint(
		PID pid, ThreadID thread_id = api::MAIN_THREAD_ID
	);

	/**
	 * @brief Returns the code position in the specified stack frame.
	 *
	 * @param frame_idx Index of the target frame.
	 *                  The active/current function has the highest frame index.
	 *                  If not provided (std::nullopt), defaults to the current (top-most) frame.
	 * @return The response containing code position or an API error.
	 */
	std::expected<response::CodePosition, ApiError> getCurrentPosition(
		PID pid, base::Optional<usize> frame_idx = {}
	);

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
	 * @brief Get an empty VMValue (initialized by zero bytes) of the given type.
	 * @note This endpoint returns a VMValue which is owned by the caller. It's the callers
	 * responsibility to call `VMValue::freeData()` on the VMValue. For more information
	 * on why this is necessary, see documentation of `vm::IVMValue::freeData()`.
	 * @return Response containing a Box containing the newly allocated VMValue of the specified type.
	 */
	std::expected<response::VMValue, ApiError> getVMValue(PID pid, const std::string& type_name);

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
	 * @brief Enables or disables a breakpoint on a given instruction of the function running in a
	 * given stack frame.
	 * @return Nothing if the breakpoint was set successfully or an API error otherwise.
	 * @note Unlike `setBreakpoint`, this addresses the function through the live stack frame, so it
	 * also works for functions that are not part of the loaded program (the synthetic
	 * `vm_start_function` and runtime expressions). The thread must be paused.
	 */
	std::expected<void, ApiError> setBreakpointAtFrame(
		PID pid, ThreadID thread_id, u64 frame_index, u64 instruction_index, bool enable
	);

	/**
	 * @brief Gets the first code collection instruction that starts in the provided file line.
	 * @return Either the mapped `CodePosition` on success, or a nullopt if no such instruction
	 * exists.
	 */
	std::expected<response::CodePosition, ApiError> mapFileLineToCodeCollectionPosition(
		PID pid, fs::File file, usize line_number
	);

	// always pauses after completion
	std::expected<ExitValue, ApiError> executeRuntimeExpr(
		PID pid, ThreadID thread_id, const code::Function& function
	);

	// always pauses after completion
	std::expected<ExitValue, ApiError> executeRuntimeExprFromFile(
		PID pid, ThreadID thread_id, fs::File file
	);

}
