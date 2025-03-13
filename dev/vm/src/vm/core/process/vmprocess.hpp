#pragma once

#include <base/optional.hpp>
#include <condition_variable>
#include <deque>
#include <listener/listener.hpp>
#include <mutex>
#include <shared_mutex>
#include <vm/api/api.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>
#include <vm/code_data/program.hpp>
#include <vm/core/process/memory/memory.hpp>
#include <vm/core/process/proc_io.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/vmthread.hpp>
#include <vm/preprocessor/preprocessor.hpp>
#include <vm/services/profiler/profiler.hpp>
#include <vm/services/reference_counter/reference_counter.hpp>
#include <vm/services/service_manager.hpp>

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

		base::Optional<vm::VMProgram> loaded_program = {};

		cpp::result<api::Response, api::LoadProgramError> loadProgram(const fs::FilePath& path);

		Memory memory;

		// @TODO: Read Processors' docs and do the TODO there...
		Preprocessor preprocessor;

		//@TODO: For now assume that bytecode validation is always turned on.
		static const bool VALIDATE_CODE = true;

		/**
		 * @brief Performs external execution request on the VCPU.
		 *
		 * This method is called by the supervisor. The possible requests include
		 * io operations, start/stop the Execution Thread or communicate with the Execution Thread.
		 *
		 * @param request Request that performs action on the Execution Thread.
		 * @return cpp::result<api::Response, api::CoreOperationError>
		 */
		cpp::result<api::Response, api::CoreOperationError>
			doRequest(const api::ExecutorRequest& request);

		/**
		 * @brief Performs external data request on the VCPU.
		 *
		 * This method is called by the supervisor.
		 * Only valid state of the VCPU for data requests is "Executing",
		 * but the executor has to be paused in some way to perform the request.
		 * It inspects the VM's memory stored in the DataManager.
		 *
		 * @param request
		 * @return cpp::result<api::Response, api::CoreOperationError>
		 */
		cpp::result<api::Response, api::CoreOperationError>
			doRequest(const api::DataRequest& request);


		cpp::result<api::Response, api::CoreOperationError> doRequest(const api::IORequest& request
		);

		/**
		 * @brief Creates new thread that runs the code in the Executor service.
		 */
		cpp::result<api::Response, api::CoreOperationError> run();

		/**
		 * @brief Joins the executing thread.
		 */
		cpp::result<api::Response, api::CoreOperationError> join();
		/**
		 * @brief Stops the executing thread (by joining it).
		 * After this method is called, the thread is removed.
		 */
		cpp::result<api::Response, api::CoreOperationError> stop();

		/**
		 * @brief Passes the input string to the executing thread.
		 * If the executing thread is paused and waiting for input, it will resume.
		 * Relevant if "uses_stdio" is false.
		 */
		cpp::result<api::Response, api::CoreOperationError> input(const api::request::Input& request
		);

		/**
		 * @brief Gets the output of the executing thread and clears the output stream.
		 * If the output stream is empty, it waits until it is not.
		 * Relevant if "uses_stdio" is false.
		 */
		cpp::result<api::Response, api::CoreOperationError> output();

		/**
		 * @brief Gets the Status of the VCPU (memory-safe).
		 *
		 * @return api::VCPUStatus
		 */
		api::ProcStatus getStatus();

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
		cpp::result<api::Response, api::CoreOperationError>
			attach(std::istream& istream = std::cin, std::ostream& ostream = std::cout);

		cpp::result<api::Response, api::CoreOperationError> detach();

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
		cpp::result<api::Response, api::CoreOperationError>
			doRequest(const api::RequestVariant& request);

		VMProcess();

		~VMProcess() final;
	};
}
