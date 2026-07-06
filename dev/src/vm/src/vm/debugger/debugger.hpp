#pragma once

#include <events/emitter.hpp>

#include <vm/api/vm.hpp>

namespace vm::debugger {
	/**
	 * @class Debugger
	 * @brief Core for the VM debugger that manages debugging sessions.
	 *
	 * This class is responsible for managing the state of debugging sessions, including
	 * breakpoints, stepping through code, and inspecting program state.
	 *
	 * It is the middleman between the VM and any user interfaces
	 * (e.g., command-line interface, graphical debugger, Debug Adapter).
	 *
	 */
	class Debugger final {
	private:
		PID                 pid;
		ProgramRunArguments main_args;

		events::Listener<api::ProcStatus> updater;
		events::Listener<std::string>     vm_output;

		// Event handlers for the debugger:

		/**
		 * @brief Emits current VM status when VM changes status
		 */
		events::Emitter<api::ProcStatus> on_status_changed;

		/**
		 * @brief Emits error message in human readable format on any error
		 */
		events::Emitter<std::string> on_error;

		/**
		 * @brief Emits the output from the VM when VM outputs
		 */
		events::Emitter<std::string> on_output;

	public:
		Debugger();
		~Debugger();
		Debugger(const Debugger&)            = delete;
		Debugger& operator=(const Debugger&) = delete;
		Debugger(Debugger&&)                 = delete;
		Debugger& operator=(Debugger&&)      = delete;

		// Method for event handlers

		/**
		 * @brief Attach Listener to Emitter that emits current VM status when VM changes status
		 */
		void attachOnStatusChangedListener(events::Listener<api::ProcStatus>& listener);

		/**
		 * @brief Attach Listener to Emitter that emits error message when any error raises
		 */
		void attachOnErrorListener(events::Listener<std::string>& listener);

		/**
		 * @brief Attach Listener to Emitter that emits error message when any error raises
		 */
		void attachOnOutputListener(events::Listener<std::string>& listener);

		// Methods to control the debugging session:

		/**
		 * @brief Runs the main function.
		 */
		std::expected<void, api::ApiError> runMain();

		/**
		 * @brief Gets the current status of the VM.
		 * @return The current status of the VM.
		 */
		[[nodiscard]] api::ProcStatus getStatus();

		/**
		 * @brief Loads files into debugger
		 */
		std::expected<void, api::ApiError> loadFiles(const std::vector<fs::File>& files);

		void setProgramArguments(const ProgramRunArguments& args);

		/**
		 * @brief Returns number of stack frames
		 */
		std::expected<u64, api::ApiError> getNumberOfStackFrames(api::ThreadID thread_id);

		/**
		 * @brief Returns variables of stack frame
		 */
		std::expected<api::response::StackFrameData, api::ApiError> getStackFrameData(
			api::ThreadID thread_id, u64 frame_index
		);

		/**
		 * @brief Pauses the VM
		 */
		std::expected<api::response::CodePosition, api::ApiError> pause();

		/**
		 * @brief Resumes the VM
		 */
		std::expected<void, api::ApiError> resume();

		/**
		 * @brief Resumes the VM
		 */
		std::expected<void, api::ApiError> evaluate(
			const fs::File&, api::ThreadID thread_id = vm::api::ThreadID(0)
		);

		/**
		 * @brief Returns current position
		 */
		std::expected<api::response::CodePosition, api::ApiError> getCurrentPosition();

		/**
		 * @brief Sets breakpoint
		 * @param function_name Name of a function to set breakpoint in.
		 * @param instr_number Index of instruction in function on which to set the breakpoint.
		 * @param enabled Decides whether the breakpoint should be enabled (inserted) or disabled
		 * (removed).
		 */
		std::expected<void, api::ApiError> setBreakpoint(
			base::StrID function_name, u64 instr_number, bool enabled = true
		);

		/**
		 * @brief Sets breakpoint
		 * @param file File to set breakpoint in.
		 * @param line Number of line in file on which to set the breakpoint.
		 * @param enabled Decides whether the breakpoint should be enabled (inserted) or disabled
		 * (removed).
		 */
		std::expected<void, api::ApiError> setBreakpoint(
			fs::File file, usize line, bool enabled = true
		);

		/**
		 * @brief Execute one FatByteCode step in the VM
		 */
		std::expected<void, api::ApiError> step();

		/**
		 * @brief Send input to the VM
		 */
		std::expected<void, api::ApiError> sendInput(const std::string& msg);
	};
}
