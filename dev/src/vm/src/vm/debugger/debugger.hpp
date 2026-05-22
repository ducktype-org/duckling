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
	 * @TODO: #2454 Implement CLI
	 */
	class Debugger final {
	private:
		PID                      pid;
		std::vector<std::string> main_args;

		events::Listener<api::ProcStatus> updater;

		// Event handlers for the debugger:

		/**
		 * @brief Emits current VM status when VM changes status
		 */
		events::Emitter<api::ProcStatus> on_status_changed;

		/**
		 * @brief Emits exit value as VMValue when VM completes execution
		 */
		events::Emitter<api::ExitValue> on_execution_completed;

		/**
		 * @brief Emits error message in human readable format on any error
		 */
		events::Emitter<std::string> on_error;

	public:
		Debugger(const std::vector<std::string>& main_args = {});
		Debugger(const fs::File& filepath, const std::vector<std::string>& main_args = {});
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
		 * @brief Attach Listener to Emitter that emits return value when VM completes execution
		 */
		void attachOnExecutionCompletedListener(events::Listener<api::ExitValue>& listener);

		/**
		 * @brief Attach Listener to Emitter that emits error message when any error raises
		 */
		void attachOnErrorListener(events::Listener<std::string>& listener);

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
		 * @brief Loads the file
		 */
		std::expected<void, api::ApiError> loadFile(const fs::File& filepath);

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
	};
}
