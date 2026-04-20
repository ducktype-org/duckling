#pragma once

#include "interface_types.hpp"

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/process/proc_io.hpp>
#include <vm/core/safe/concurrency/gil.hpp>
#include <vm/core/safe/concurrency/synchronization_primitives.hpp>
#include <vm/core/safe/memory/pointer.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>

#include <expected>
#include <shared_mutex>
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
	 *
	 */
	class IVMProcess {
	protected:
		PID                              my_pid;
		ProcIO                           io;
		base::Optional<ProcIORedirecter> io_redirecter;
		// See: https://en.cppreference.com/w/cpp/io/ios_base/Init
		std::ios_base::Init cin_cout_init;

		api::ProcStatus             status;
		std::shared_mutex           rw_status;
		std::condition_variable_any status_cv;

		/**
		 * @brief Emits current status when VM changes status
		 */
		events::Emitter<api::ProcStatus> on_status_change;

		IVMProcess(PID my_pid);

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

		// @TODO: #2400 Remove this
		void setStatus(const api::ProcStatus& new_status) noexcept;

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
