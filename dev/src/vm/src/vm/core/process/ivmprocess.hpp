#pragma once

#include "interface_types.hpp"

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/status.hpp>
#include <vm/api/data/thread_id.hpp>
#include <vm/core/process/proc_io.hpp>
#include <vm/core/process/process_state.hpp>
#include <vm/core/thread/thread_state.hpp>

#include <condition_variable>
#include <expected>
#include <mutex>
#include <variant>

namespace vm {

	class VmValue;

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

	protected:
		PID                              my_pid;
		ProcIO                           io;
		base::Optional<ProcIORedirecter> io_redirecter;
		// See: https://en.cppreference.com/w/cpp/io/ios_base/Init
		std::ios_base::Init cin_cout_init;

		/**
		 * @brief Per-VMThread states and stop flag. Used to calculate the aggregate state of
		 * VMProcess via `aggregate`.
		 * Protected by `threads_states_mutex`.
		 */
		process_sm::ProcessAggregationState threads_states{};
		mutable std::mutex                  threads_states_mutex;

		/**
		 * @brief CV for waiting for a change of the subthread states. Used by `waitForProcessState`.
		 */
		mutable std::condition_variable threads_states_changed;

		/**
		 * @brief The most recent aggregate of VMProcesses VMThread states.
		 * Protected by `threads_states_mutex`.
		 */
		process_sm::process_state::ProcessState last_process_state{
			process_sm::process_state::NotStarted{}
		};

		/**
		 * @brief Counter incremented on every thread -> process state update.
		 * Used for waiting for a fresh state. For example when a process is Paused, we run it and
		 * pause it again. The quick Pause-Running-Pause change might not have been observed and
		 * theres no way to distinguish between the two Paused states.
		 * @see `waitForFreshProcessState`
		 * Guarded by `threads_states_mutex`.
		 */
		u64 process_state_change_counter{ 0 };

		/**
		 * @brief Emits after the process status has changed.
		 */
		events::Emitter<api::ProcStatus> on_status_changed;

		IVMProcess(PID my_pid);


		/**
		 * @brief Returns a copy of the current aggregated process state.
		 */
		[[nodiscard]] ProcessState getProcessState() const;

		/**
		 * @brief Get a copy of the VMProcess state change counter.
		 * Used with `waitForFreshProcessState`.
		 */
		[[nodiscard]] u64 getProcessStateChangeCount() const {
			std::lock_guard<std::mutex> lock(threads_states_mutex);
			return process_state_change_counter;
		}

		/**
		 * @brief Blocks until aggregate(threads_states) satisfies pred, then returns that state.
		 * Releases threads_states_mutex while waiting; safe to call from any thread.
		 */
		template<typename Pred>
		[[nodiscard]] ProcessState waitForProcessState(Pred&& pred) const {
			std::unique_lock<std::mutex> lock(threads_states_mutex);
			threads_states_changed.wait(lock, [&] {
				return std::forward<Pred>(pred)(process_sm::aggregate(threads_states));
			});
			return process_sm::aggregate(threads_states);
		}

		/**
		 * @brief Blocks until the state has advanced past `since` and satisfies pred.
		 * Useful, when a process state may change fast and we won't observe it. For example on
		 * Paused->Running->Paused. We can't distinguish between the two Paused states if not for
		 * the `since`.
		 */
		template<typename Pred>
		[[nodiscard]] ProcessState waitForFreshProcessState(u64 since, Pred&& pred) const {
			std::unique_lock<std::mutex> lock(threads_states_mutex);
			threads_states_changed.wait(lock, [&] {
				return process_state_change_counter > since
				    && std::forward<Pred>(pred)(process_sm::aggregate(threads_states));
			});
			return process_sm::aggregate(threads_states);
		}


	private:
		/**
		 * @brief Called by the thread state machine listener linked in `adoptThread`.
		 * Updates `threads_states`, recomputes the aggregate, emits `on_status_changed` if the
		 * process-level state changed. Cascades a Kill to all threads on panic.
		 */
		void onThreadStateChanged(
			api::ThreadID tid, const thread_sm::thread_state::ThreadState& new_state
		);

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
		 * @brief Stops the executing thread (by joining it).
		 * After this method is called, the thread is removed.
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
			api::ThreadID thread_id
		) = 0;

		virtual std::expected<api::Response, api::ApiError> getNumberOfCurrentStackFrames(
			api::ThreadID thread_id
		) = 0;

		virtual std::expected<api::Response, api::ApiError> getStackFrameData(
			api::ThreadID thread_id, u64 frame_index
		) = 0;

		virtual void notifyPausedVMThread(api::ThreadID thread_id) = 0;

		virtual void waitForBreakpoint() = 0;

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
		 * Called without holding `rw_status` lock.
		 */
		virtual void onTerminalStatus(const api::ProcStatus&) noexcept {}

		/**
		 * @brief Posts Stop to all threads. Non blocking.
		 */
		virtual void requestStopAllThreads() noexcept = 0;


		/**
		 * @brief Links a VMThread into the VMProcesses state aggregation.
		 *
		 * Saves the VMThreads initial state and subscribes a listener on the thread's state machine
		 * that calls `onThreadStateChanged` on every thread state transition.
		 * @note: Must be called once per thread, before it is spawned.
		 */
		void adoptThread(IVMThread& thread);

		/**
		 * @brief Performs a specific VMProcess state change command. Like run, stop, kill, etc.
		 */
		base::Optional<api::ApiError> applyCommand(const ProcessEvent& event);

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
		ProcIO& getIO();

		/**
		 * @brief Entry point to perform requests on the process.
		 */
		std::expected<api::Response, api::ApiError> doRequest(const api::RequestVariant& request);

		/**
		 * @brief Get the PID of the process.
		 */
		[[nodiscard]] PID getPID() const;

		/**
		 * @brief Creates a VmValue of a given type and registers it in this VMProcess
		 * The VmValue is owned by the VMProcess. VmValues created with this function are freed when
		 * the process is deinitialized.
		 *
		 * @param type The type of the data stored in the newly created VmValue.
		 * @param src The pointer to the data used to fill the newly created VmValue. If not
		 * specified, created VmValue will be empty.
		 * @return A non-owning, modifiable reference to the new VmValue.
		 */
		virtual Ref<VmValue> createVmValue(TypeCRef type)              = 0;
		virtual Ref<VmValue> createVmValue(TypeCRef type, Pointer src) = 0;

		/**
		 * @brief Creates a VmValue of a given type and transfers ownership to the caller.
		 * The caller is expected to free the VmValue.
		 *
		 * @param type The type of the data stored in the newly created VmValue.
		 * @param src The pointer to the data used to fill the newly created VmValue. If not
		 * specified, created VmValue will be empty.
		 * @return A Box referencing the newly created VmValue.
		 */
		virtual Box<VmValue> createOwnedVmValue(TypeCRef type)              = 0;
		virtual Box<VmValue> createOwnedVmValue(TypeCRef type, Pointer src) = 0;

		virtual ~IVMProcess() = default;
	};
}
