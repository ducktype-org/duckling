#include <events/emitter.hpp>

#include "vm/core/vmvalue/vmvalueref.hpp"
#include <vm/api/vm.hpp>

namespace vm::debugger {

	struct StackFrameHook {
		u64 thread_id;
		u64 frame_id;

		bool operator==(const StackFrameHook&) const = default;
	};

	struct VariableHook {
		VMValueRef ref;

		bool operator==(const VariableHook&) const = default;
	};

	struct VariablesReference {
		struct Nothing {};

		u64                                                 id;
		std::variant<StackFrameHook, VariableHook, Nothing> vr;

		bool operator==(const VariablesReference&) const = default;
	};

	struct StackFrameInfo {
		u64         frame_id;
		base::StrID function_name;
		u64         variables_reference;
	};

	struct VariableInfo {
		base::StrID name;
		std::string value;
		std::string type;
		u64         variables_reference;  // 0 if none
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

		std::vector<VariablesReference> enumerated_variables_references;

		void clearEnumeratedVariablesReferences();

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

		std::vector<StackFrameInfo> enumerateFrames(u64 thread_id = 0);

		std::vector<VariableInfo> dereferenceVariablesReference(u64 variables_reference);
		/**
		 * @brief Pauses the VM
		 */
		void pause();

		/**
		 * @brief Resumes the VM
		 */
		void resume();
	};
}
