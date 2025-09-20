#pragma once

#include "interface_types.hpp"

#include <base/optional.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/status.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/memory/memory.hpp>
#include <vm/core/process/proc_io.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/vmthread.hpp>
#include <vm/loader/loader.hpp>
#include <vm/services/profiler/profiler.hpp>
#include <vm/services/reference_counter/reference_counter.hpp>
#include <vm/services/service_manager.hpp>

#include <condition_variable>
#include <deque>
#include <expected>
#include <shared_mutex>
#include <string>
#include <variant>
#include <vector>

namespace vm::loader {
	class Loader;
}

namespace vm {

	/**
	 * @brief The API for using the virtual process of the VM.
	 * It manages process'es data and services.
	 *
	 * VMProcess is an abstract concepts that represents the program's execution environment.
	 *
	 * @note The code in this class is executed in the supervisor's thread.
	 *
	 * It is responsible for loading and parsing of the program,
	 * creating and resetting the Execution Thread,
	 * setting the status of the execution (pause, stop, run),
	 * managing the input and output of the executing thread and some more.
	 *
	 * Only execution of the code is done in the separate thread,
	 * loading and parsing of the program is done in the caller's thread.
	 */
	class VMProcess final {
		friend class VmValue;

	private:
		PID my_pid;

		std::shared_mutex rw_global;

		api::ProcStatus             status;
		std::shared_mutex           rw_status;
		std::condition_variable_any status_cv;

		// See: https://en.cppreference.com/w/cpp/io/ios_base/Init
		std::ios_base::Init cin_cout_init;

		/**
		 * @brief The program being executed by this process.
		 * Holds a constant reference to the LowVMProgram stored in the processes compiler module or
		 * nullptr if no code was loaded.
		 */
		MCRef<vm::low::LowVMProgram> loaded_program = nullptr;

		Memory memory;


		/**
		 * @brief A loader instance for this VMProcess. Stores the high level and low level
		 * representation of the currently executed program. `loaded_program` references the low
		 * representation which exists in this class.
		 */
		loader::Loader loader{};

		/**
		 * @brief Storage for all VmValues which belong to this process.
		 * @note Lifetime of these VmValues is controlled by this process. They will be destructed
		 * when process is deinitialized.
		 */
		std::vector<Box<VmValue>> owned_vm_values;

		// @TODO: Improve this....
		std::deque<VMThread> vm_threads;

		/**
		 * @brief Loads the program from a given source into the current loader program state,
		 * recompiles the program as a whole and moves an updated program into VMProcesses memory.
		 */
		std::expected<api::Response, api::LoadProgramError> loadProgram(
			const std::variant<std::vector<fs::File>, code::CodeCollection>& source
		);

		/**
		 * @brief Creates new thread that runs a function in the Executor service.
		 */
		std::expected<api::Response, api::ApiError> runFunction(
			const std::string& func_name, const RunArguments& run_arguments
		);

		/**
		 * @brief Joins the executing thread.
		 */
		std::expected<api::Response, api::ApiError> join();

		/**
		 * @brief Stops the executing thread (by joining it).
		 * After this method is called, the thread is removed.
		 */
		std::expected<api::Response, api::ApiError> stop();

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

		base::Optional<api::ApiError> validateMemoryRequest();

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
		std::expected<api::Response, api::StateError> getExitCode();

		/**
		 * @brief Holds all services. When it's constructed, it initializes all services.
		 */
		ServiceManager service_manager = ServiceManager();

		ProcIO                           io;
		base::Optional<ProcIORedirecter> io_redirecter;

		/**
		 * @brief Attaching means all IO is interactive, input is read from stdin, output
		 * @brief is automatically forwarded to stdout.
		 */
		std::expected<api::Response, api::ApiError> attach(
			std::istream& istream = std::cin, std::ostream& ostream = std::cout
		);

		std::expected<api::Response, api::ApiError> detach();

		VMThread& getMainVMThread();


	public:
		void onEvent(const api::ProcStatus& event) noexcept;

		Memory& getMemory();

		/**
		 * Can be safely called from Execution Thread only
		 */
		ServiceManager& getServices();

		ProcIO& getIO();

		/**
		 * @brief Entry point to perform requests on the process.
		 */
		std::expected<api::Response, api::ApiError> doRequest(const api::RequestVariant& request);

		PID getPID() const;

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
		Ref<VmValue> createVmValue(TypeCRef type);
		Ref<VmValue> createVmValue(TypeCRef type, Pointer src);

		/**
		 * @brief Creates a VmValue of a given type and transfers ownership to the caller.
		 * The caller is expected to free the VmValue.
		 *
		 * @param type The type of the data stored in the newly created VmValue.
		 * @param src The pointer to the data used to fill the newly created VmValue. If not
		 * specified, created VmValue will be empty.
		 * @return A Box referencing the newly created VmValue.
		 */
		Box<VmValue> createOwnedVmValue(TypeCRef type);
		Box<VmValue> createOwnedVmValue(TypeCRef type, Pointer src);

		VMProcess(PID my_pid);

		~VMProcess();
	};
}
