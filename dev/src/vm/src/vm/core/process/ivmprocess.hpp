#pragma once

#include "interface_types.hpp"

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/status.hpp>
#include <vm/api/data/thread_id.hpp>
#include <vm/bytecode/validator/valid_type/valid_type_id.hpp>
#include <vm/core/process/proc_io.hpp>
#include <vm/core/process/process_state.hpp>
#include <vm/core/process/state_table.hpp>
#include <vm/core/thread/thread_state.hpp>

#include <expected>
#include <variant>

namespace vm {

	class IVMValue;

	/**
	 * @brief The API for using the virtual process of the VM.
	 * It manages process's data, loader and threads.
	 *
	 * This is an abstract class that represents the program's execution environment.
	 *
	 * @note The implementations of this class run the loading and parsing of the program,
	 * in the caller's thread, only the execution of the program in a separate thread.
	 *
	 * It is responsible for loading and parsing of the program,
	 * creating and resetting the Execution Thread,
	 * setting the status of the execution (pause, stop, run),
	 * managing the input and output of the executing thread and some more.
	 */
	class IVMProcess {
	public:
		using ProcessState = process_sm::process_state::ProcessState;
		using ProcessEvent = process_sm::process_event::ProcessEvent;
		using ThreadEvent  = thread_sm::thread_event::ThreadEvent;

	protected:
		PID                              my_pid;
		ProcIO                           io;
		base::Optional<ProcIORedirecter> io_redirecter;
		// See: https://en.cppreference.com/w/cpp/io/ios_base/Init
		std::ios_base::Init cin_cout_init;

		/**
		 * @brief The single source of truth for all VMThread states of this process.
		 */
		ProcessStateTable state_table;

		api::ExecutionConfig execution_config;

		/**
		 * @brief Emits after the process status has changed.
		 */
		events::Emitter<api::ProcStatus> on_status_changed;

		IVMProcess(PID my_pid);

		/**
		 * @brief Returns a copy of the current calculated process state.
		 */
		[[nodiscard]] ProcessState getProcessState() const { return state_table.aggregate(); }

		/**
		 * @brief Blocks until the process state satisfies pred, then returns it. Can be called both
		 * from VMProcess and VMThread.
		 */
		template<typename Pred>
		[[nodiscard]] ProcessState waitForProcessState(Pred&& pred) const {
			return state_table.wait([&](const ProcessStateTable::Snapshot& snapshot) {
				return std::forward<Pred>(pred)(snapshot.processState());
			});
		}

		/**
		 * @brief Validates the current API request against the aggregate state.
		 * @return Empty optional on success, ApiError if the request is invalid in the current state.
		 */
		[[nodiscard]] base::Optional<api::ApiError> validateRequest(const ProcessEvent& event) const;

		/**
		 * @brief Prepares a run (or a rerun). Called when the process is terminal, moves all
		 * threads back to `NotStarted` and clears the stop-all-threads flag.
		 * @return An empty optional when the process is ready to run - either because it was not
		 * terminal or because the reset succeeded. An `ApiError` when the reset was refused (i.e.
		 * thread is still active)
		 */
		[[nodiscard]] base::Optional<api::ApiError> prepareRun();

	private:
		/**
		 * @brief Loads the program from a given source into the current loader program state,
		 * recompiles the program as a whole and moves an updated program into VMProcesses memory.
		 */
		virtual std::expected<api::Response, api::LoadProgramError> loadProgram(
			const std::variant<std::vector<fs::File>, code::CodeCollection>& source
		) = 0;

		/**
		 * @brief Creates new thread that runs a function.
		 */
		virtual std::expected<api::Response, api::ApiError> runFunction(
			const std::string& func_name, const RunArguments& run_arguments
		) = 0;

		/**
		 * @brief Runs a function and waits for it to finish.
		 * @note Does not create a new thread, runs the function in the current execution thread.
		 * @return The exit value of the function if it was ran successfully or an API error
		 * otherwise.
		 */
		virtual std::expected<api::Response, api::ApiError> runFunctionAwait(
			const std::string& func_name, const RunArguments& run_arguments
		) = 0;

		/**
		 * @brief Joins the executing thread.
		 */
		virtual std::expected<api::Response, api::ApiError> join(api::ThreadID thread_id) = 0;

		/**
		 * @brief Stops the executing threads (by joining them).
		 */
		virtual std::expected<api::Response, api::ApiError> stop() = 0;

		/**
		 * @brief Passes the input string to the executing thread.
		 * If the executing thread is paused and waiting for input, it will resume.
		 * Relevant if "uses_stdio" is false.
		 */
		std::expected<api::Response, api::ApiError> input(const api::request::Input& request);

		/**
		 * @brief Gets the output of the executing thread and clears the output stream.
		 * If the output stream is empty, it waits until it is not.
		 * Relevant if "uses_stdio" is false.
		 */
		std::expected<api::Response, api::ApiError> output();

	protected:
		/**
		 * @brief Gets the status of the process (memory-safe).
		 *
		 * @return api::ProcStatus
		 */
		api::ProcStatus getStatus();


		/**
		 * @brief Returns exit code of the process - i.e. return value of `main` bytecode function.
		 *
		 * @return api::Response
		 */
		virtual std::expected<api::Response, api::StateError> getExitCode() = 0;

		/**
		 * @brief Expects the process to be stopped and asks memory module if the memory is valid.
		 * For more information about execution's validation,
		 * see Memory::validateMemoryState's description.
		 */
		virtual std::expected<api::Response, api::ApiError> deinitAndValidate() = 0;

		/**
		 * @brief Attaching means all IO is interactive, input is read from stdin, output
		 * @brief is automatically forwarded to stdout.
		 */
		virtual std::expected<api::Response, api::ApiError> attach(
			std::istream& istream, std::ostream& ostream
		);

		virtual std::expected<api::Response, api::ApiError> detach();

		// Virtual thread dependencies for doRequest
		virtual base::Optional<api::ApiError> pauseVMThread(api::ThreadID thread_id) = 0;

		virtual base::Optional<api::ApiError> resumeVMThread(api::ThreadID thread_id) = 0;

		virtual base::Optional<api::ApiError> stepVMThread(api::ThreadID thread_id) = 0;

		virtual std::expected<api::Response, api::ApiError> getVMThreadCurrentPosition(
			api::ThreadID thread_id, base::Optional<usize> frame_idx = std::nullopt
		) = 0;

		virtual std::expected<api::Response, api::ApiError> getNumberOfCurrentStackFrames(
			api::ThreadID thread_id
		) = 0;

		virtual std::expected<api::Response, api::ApiError> getStackFrameData(
			api::ThreadID thread_id, u64 frame_index
		) = 0;

		virtual void notifyVMThreadWaiters(api::ThreadID thread_id) = 0;

		virtual void waitForBreakpoint() = 0;

		virtual std::expected<api::Response, api::ApiError> setExecutionConfig(
			const api::ExecutionConfig& config
		) = 0;

		/**
		 * @brief Gets type metadata for a given type name. Type must be defined in the loaded
		 * program.
		 */
		virtual std::expected<api::Response, api::ApiError> getTypeMetadata(
			const std::string& type_name
		) = 0;

		/**
		 * @brief Gets empty VMValue for a given type name.
		 */
		virtual std::expected<api::Response, api::ApiError> getVMValueForType(
			const std::string& type_name
		) = 0;

		virtual std::vector<api::ThreadID> getAllThreadIDs() = 0;

		virtual api::ThreadID getMainThreadID() = 0;

		/**
		 * @brief Hook called after process status changes to a terminal one.
		 * Called by the state table's `onStateChangeCallback`.
		 */
		virtual void onTerminalStatus(const api::ProcStatus&) noexcept {}

		/**
		 * @brief Posts Stop to all threads. Non blocking.
		 */
		virtual void requestStopAllThreads() noexcept = 0;

		/**
		 * @brief Enables or disables breakpoint on a given instruction in a given function.
		 * @note Enabling a breakpoint on an instruction that already has a breakpoint or disabling
		 * a breakpoint on an instruction that doesn't have a breakpoint is considered successful
		 * and doesn't return an error.
		 */
		virtual std::expected<api::Response, api::ApiError> setBreakpoint(
			base::StrID function_name, usize instruction_index, bool enable
		) = 0;

		virtual std::expected<api::Response, api::ApiError> mapFileLineToCodeCollectionPosition(
			const fs::File& file, usize line_number
		) = 0;

	public:
		IVMProcess(const IVMProcess&)            = delete;
		IVMProcess(IVMProcess&&)                 = delete;
		IVMProcess& operator=(const IVMProcess&) = delete;
		IVMProcess& operator=(IVMProcess&&)      = delete;

		ProcIO& getIO();

		/**
		 * @brief The state table holding all VMThread states of this process.
		 */
		[[nodiscard]] ProcessStateTable& getStateTable() { return state_table; }

		[[nodiscard]] const ProcessStateTable& getStateTable() const { return state_table; }

		/**
		 * @brief Commits a thread state transition. Aborts on invalid transition, since it's a
		 * fatal DVM error. Propagates a cascade kill to all threads when the event was a first
		 * panic. Called by `IVMThread::commitEvent`.
		 */
		void commitThreadEvent(api::ThreadID tid, const ThreadEvent& event);

		/**
		 * @brief Entry point to perform requests on the process.
		 */
		virtual std::expected<api::Response, api::ApiError> doRequest(
			const api::RequestVariant& request
		);

		/**
		 * @brief Get the PID of the process.
		 */
		[[nodiscard]] PID getPID() const;

		/**
		 * @brief Creates an empty VMValue of a given type and registers it in this VMProcess.
		 * The VMValue is owned by the VMProcess. VMValues created with this function are freed when
		 * the process is deinitialized.
		 *
		 * @param type_id ID of the type of the data stored in the new VMValue.
		 * @return A non-owning, modifiable reference to the new VMValue.
		 */
		virtual Ref<IVMValue> createVMValue(code::valid_type::ValidTypeID type_id) = 0;

		/**
		 * @brief Creates an empty VMValue of a given type and transfers ownership to the caller.
		 * The caller is expected to free the VMValue.
		 *
		 * @param type_id ID of the type of the data stored in the new VMValue.
		 * @return A Box referencing the newly created VMValue.
		 */
		virtual Box<IVMValue> createOwnedVMValue(code::valid_type::ValidTypeID type_id) = 0;

		virtual ~IVMProcess() = default;
	};
}
