#pragma once

#include <events/emitter.hpp>

#include <vm/api/vm.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalue.hpp>
#include <vm/debugger/mapper.hpp>

#include <deque>
#include <optional>

namespace vm::debugger {
	struct CodePosition final: public api::response::CodePosition {
		base::Optional<dia::SourcePosition> mapped_position;
	};

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
		Mapper              mapper;

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

		/**
		 * @brief Listeners for expressions that paused on a breakpoint and did not finish yet.
		 * They stay attached to their result emitter and print the values once the expression
		 * completes. Kept alive here, because an emitter only holds raw pointers to its listeners.
		 */
		std::deque<events::Listener<std::vector<Ref<SafeVMValue>>>> pending_expr_result_listeners;

		CodePosition mapCodePosition(const api::response::CodePosition& pos);

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

		std::expected<void, std::variant<api::ApiError, std::string>> loadDefault(
			base::Optional<fs::FilePath> prefix = std::nullopt
		);

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
		std::expected<CodePosition, api::ApiError> pause();

		/**
		 * @brief Resumes the VM
		 */
		std::expected<void, api::ApiError> resume();

		/**
		 * @brief Evaluates a runtime expression read from `file` on the given thread and prints
		 * the returned values. If the expression stops on a breakpoint, its result is printed once
		 * it completes instead.
		 */
		std::expected<void, api::ApiError> evaluate(
			const fs::File& file, api::ThreadID thread_id = vm::api::ThreadID(0)
		);

		/**
		 * @brief Returns the code position in the specified stack frame.
		 *
		 * @param frame_idx Index of the target frame.
		 *                  The active/current function has the highest frame index.
		 *                  If not provided (std::nullopt), defaults to the current (top-most) frame.
		 **/
		std::expected<CodePosition, api::ApiError> getCurrentPosition(
			base::Optional<usize> frame_idx = std::nullopt
		);


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
		 *
		 * @return The position the VM stopped at, or nothing when the step ended the program, or an
		 * API error.
		 */
		std::expected<base::Optional<CodePosition>, api::ApiError> step();

		/**
		 * @brief Execute multiple FatByteCode steps in the VM until next position in source file is
		 * reached (or just steps if there is no mapping available)
		 *
		 * @return The position the VM stopped at, or nothing when the steps ended the program, or
		 * an API error.
		 */
		std::expected<base::Optional<CodePosition>, api::ApiError> mappedStep();

		/**
		 * @brief Send input to the VM
		 */
		std::expected<void, api::ApiError> sendInput(const std::string& msg);

		const Mapper& getMapper();
	};
}
