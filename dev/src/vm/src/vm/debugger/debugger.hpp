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
		vm::PID                  pid;
		std::vector<std::string> main_args;

		events::Listener<vm::api::ProcStatus> updater;

		// Event handlers for the debugger:

		/**
		 * @brief Emits current VM status when VM changes status
		 */
		events::Emitter<vm::api::ProcStatus> on_vm_changes_status;

		/**
		 * @brief Emits exit value as VMValue when VM completes execution
		 */
		events::Emitter<vm::api::ExitValue> on_vm_completes_execution;

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
		void attachOnVMChangesStatusListener(events::Listener<vm::api::ProcStatus>& listener);

		/**
		 * @brief Attach Listener to Emitter that emits return value when VM completes execution
		 */
		void attachOnVMCompletesExecutionListener(events::Listener<vm::api::ExitValue>& listener);

		/**
		 * @brief Attach Listener to Emitter that emits error message when any error raises
		 */
		void attachOnErrorListener(events::Listener<std::string>& listener);

		// Methods to control the debugging session:

		/**
		 * @brief Runs the main function.
		 */
		void runMain();

		/**
		 * @brief Gets the current status of the VM.
		 * @return The current status of the VM.
		 */
		[[nodiscard]] vm::api::ProcStatus getStatus();

		/**
		 * @brief Loads the file
		 */
		void loadFile(const fs::File& filepath);
	};
}
