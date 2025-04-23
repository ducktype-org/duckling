#pragma once

#include <listener/listener.hpp>

#include <base/optional.hpp>

#include <vm/api/api.hpp>
#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/state_error.hpp>
#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>
#include <vm/core/process/memory/memory.hpp>
#include <vm/core/process/proc_io.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/vmthread.hpp>
#include <vm/services/profiler/profiler.hpp>
#include <vm/services/reference_counter/reference_counter.hpp>
#include <vm/services/service_manager.hpp>

#include <condition_variable>
#include <deque>
#include <expected>
#include <shared_mutex>

namespace vm::loader {
	class Loader;
}

namespace vm {
	using ServiceManager = ServiceManagerDef<ReferenceCounter, Profiler>;

	/**
	 * @brief The API for using the virtual process of the VM.
	 * It manages process'es data and services.
	 *
	 * VMProcess is an abstract concepts that represents the program's execution environment.
	 *
	 * @note The code in this class is executed in the supervisor's thread.
	 *
	 * It is responsible for loading and parsing of the program,
	 * creating and reseting the Execution Thread,
	 * setting the status of the execution (pause, stop, run),
	 * managing the input and output of the executing thread and some more.
	 *
	 * Only execution of the code is done in the separate thread,
	 * loading and parsing of the program is done in the caller's thread.
	 */
	class VMProcess final: public Listener<api::ProcStatus> {
	private:
		std::shared_mutex rw_global;

		api::ProcStatus             status;
		std::shared_mutex           rw_status;
		std::condition_variable_any status_cv;

		// See: https://en.cppreference.com/w/cpp/io/ios_base/Init
		std::ios_base::Init cin_cout_init;

		base::Optional<vm::low::LowVMProgram> loaded_program = {};

		std::expected<api::Response, api::LoadProgramError> loadProgram(const fs::FilePath& path);

		Memory memory;

		Box<loader::Loader> loader;

		//@TODO: For now assume that bytecode validation is always turned on.
		static constexpr const bool VALIDATE_CODE = true;

		/**
		 * @brief Performs external execution request on the VCPU.
		 *
		 * This method is called by the supervisor. The possible requests include
		 * io operations, start/stop the Execution Thread or communicate with the Execution Thread.
		 *
		 * @param request Request that performs action on the Execution Thread.
		 * @return std::expected<api::Response, api::CoreOperationError>
		 */
		std::expected<api::Response, api::CoreOperationError> doRequest(
			const api::ExecutorRequest& request
		);

		/**
		 * @brief Performs external data request on the VCPU.
		 *
		 * This method is called by the supervisor.
		 * Only valid state of the VCPU for data requests is "Executing",
		 * but the executor has to be paused in some way to perform the request.
		 * It inspects the VM's memory stored in the DataManager.
		 *
		 * @param request
		 * @return std::expected<api::Response, api::CoreOperationError>
		 */
		std::expected<api::Response, api::CoreOperationError> doRequest(
			const api::DataRequest& request
		);

		std::expected<api::Response, api::CoreOperationError> doRequest(const api::IORequest& request
		);

		/**
		 * @brief Creates new thread that runs the code in the Executor service.
		 */
		std::expected<api::Response, api::CoreOperationError> run(const std::vector<std::string>& args
		);

		/**
		 * @brief Joins the executing thread.
		 */
		std::expected<api::Response, api::CoreOperationError> join();

		/**
		 * @brief Stops the executing thread (by joining it).
		 * After this method is called, the thread is removed.
		 */
		std::expected<api::Response, api::CoreOperationError> stop();

		/**
		 * @brief Passes the input string to the executing thread.
		 * If the executing thread is paused and waiting for input, it will resume.
		 * Relevant if "uses_stdio" is false.
		 */
		std::expected<api::Response, api::CoreOperationError> input(const api::request::Input& request
		);

		/**
		 * @brief Gets the output of the executing thread and clears the output stream.
		 * If the output stream is empty, it waits until it is not.
		 * Relevant if "uses_stdio" is false.
		 */
		std::expected<api::Response, api::CoreOperationError> output();

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
		std::expected<api::Response, api::CoreOperationError> attach(
			std::istream& istream = std::cin, std::ostream& ostream = std::cout
		);

		std::expected<api::Response, api::CoreOperationError> detach();

		// @TODO: Improve this....
		std::deque<VMThread> vm_threads;

		VMThread& getMainVMThread();

	public:
		void onEvent(const api::ProcStatus& event) noexcept override;

		Memory& getMemory();

		/**
		 * Can be safely called from Execution Thread only
		 */
		ServiceManager& getServices();

		ProcIO& getIO();


		// Each of the following methods can be called concurrently, so they should synchronize
		// resources.
		/**
		 * @brief Entry point to perform requests on the process.
		 */
		std::expected<api::Response, api::ApiError> doRequest(const api::RequestVariant& request);

		VMProcess();

		~VMProcess() final;
	};
}
