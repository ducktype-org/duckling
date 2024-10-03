#pragma once

#include <shared_mutex>
#include <thread>
#include <memory>
#include <base/optional.hpp>
#include <api/vm.hpp>

#include <ostream>
#include <istream>
#include <iostream>

namespace vm {
	/**
	 * @brief The virtual CPU of the VM. It governs the thread that executes the code, and manages it's data and services.
	 *
	 * @note The code in this class is executed in the supervisor's thread.
	 * 
	 * It is responsible for creating and reseting the Exection Thread, for
	 * setting the status of the execution (pause, stop, run) and for managing the input and output 
	 * of the executing thread.
	 */
	class VCPU: public Listener<api::VCPUStatus> {
	private:
		std::shared_mutex rwGlobal;

		std::unique_ptr<std::thread> coreThread;

		std::condition_variable_any status_cv;
		std::shared_mutex           rwStatus;
		api::VCPUStatus             status;

		// See: https://en.cppreference.com/w/cpp/io/ios_base/Init
		std::ios_base::Init cin_cout_init;

		// @TODO: In future, never use stdio. Even CLI should use custom input/output and manage IO
		// on its own. It will help with the need to support other concurrent processes and
		// inserting CLI's commands
		bool                                uses_stdio;
		std::mutex                          input_mutex;
		std::mutex                          output_mutex;
		std::condition_variable             output_empty_cv;
		base::unique_ptr<std::stringstream> input_stream  = nullptr;
		base::unique_ptr<std::stringstream> output_stream = nullptr;

		base::Optional<vm::Code> loadedCode = {};

		cpp::result<api::Response, api::LoadProgramError> loadProgram(const fs::FilePath& path);

		/**
		 * @brief Performs external execution request on the VCPU.
		 * 
		 * This method is called by the supervisor. The possible requests include 
		 * io operations, start/stop the Exection Thread or communicate with the Exection Thread.
		 * 
		 * @param request Request that performs action on the Exection Thread.
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
		 * It inspects the VM's memory.
		 * 
		 * @param request 
		 * @return cpp::result<api::Response, api::CoreOperationError> 
		 */
		cpp::result<api::Response, api::CoreOperationError>
			doRequest(const api::DataRequest& request);

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
		api::VCPUStatus getStatus();


		/**
		 * Service and data manager constructors can depend on
		 * VCPU, and should be constructor as a last one in VCPU
		 *
		 * Also ServiceManager initialization depend on DataManager, so it has to be
		 * initialized before serviceManager.
		 *
		 * This design is not perfect, and might be changed in the future.
		 */
		DataManager    dataManager;
		/**
		 * @brief Holds all services. When it's constructed, it initializes all services.
		 */
		ServiceManager serviceManager;

	public:
		/**
		 * For internal use only
		 * Usage:
		 * Supervisor::get().onEvent(status);
		 *
		 * Name of this method may be misleading
		 */
		void onEvent(const api::VCPUStatus& event) noexcept override;

		/**
		 * Can be safely called from Execution Thread only
		 */
		DataManager& getData();

		/**
		 * Can be safely called from Execution Thread only
		 */
		ServiceManager& getServices();

		/**
		 * For executor use only
		 */
		template<class T>
		T getInput() {
			T     v{};
			auto& exec = serviceManager.get<Executor>();
			if (uses_stdio) {
				std::cin >> v;
			} else {
				std::unique_lock lock(input_mutex);
				exec.waitUntilNotPausedAndCondition(lock, [this, &exec] {
					return !exec.isAlive() || !input_stream->str().empty();
				});
				if (exec.isAlive()) (*input_stream) >> v;
			}
			return v;
		}

		template<class T>
		void writeOutput(const T& v) {
			if (uses_stdio) {
				std::cout << v;
			} else {
				std::unique_lock lock(output_mutex);
				(*output_stream) << v;
				output_empty_cv.notify_one();
			}
		}

		// For external API

		// Each of the following methods can be called concurrently, so they should synchronize
		// resources.
		/**
		 * @brief Entry point to perform requests on the VCPU.
		 */
		cpp::result<api::Response, api::CoreOperationError>
			doRequest(const api::RequestVariant& request);

		VCPU(bool use_stdio):
			  status(api::ExecutionNotStarted{}),
			  uses_stdio(use_stdio),
			  serviceManager(*this){};
		virtual ~VCPU();
	};
}
