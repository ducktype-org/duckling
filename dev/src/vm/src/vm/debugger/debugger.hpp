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
	 * It will interact with the VM API and be the middleman between the VM and any user interfaces
	 * (e.g., command-line interface, graphical debugger, Debug Adapter).
	 */
	class Debugger {
	private:
		vm::PID                  pid;
		std::vector<std::string> main_args;

		events::Listener<vm::api::ProcStatus> updater;

	public:
		Debugger(const fs::File& filepath, const std::vector<std::string>& main_args = {});
		Debugger(const Debugger&)            = delete;
		Debugger& operator=(const Debugger&) = delete;
		Debugger(Debugger&&)                 = delete;
		Debugger& operator=(Debugger&&)      = delete;

		// Event handlers for the debugger:
		events::Emitter<vm::api::ProcStatus> on_vm_status_change;

		// Methods to control the debugging session:

		/**
		 * @brief Runs the main function.
		 */
		void runMain();

		/**
		 * @brief Gets the current status of the VM.
		 * @return The current status of the VM.
		 */
		[[nodiscard]] vm::api::ProcStatus getStatus() const;
	};
}
