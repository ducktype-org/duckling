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

		cpp::result<api::Response, api::CoreOperationError>
			doRequest(const api::ExecutorRequest& request);
		cpp::result<api::Response, api::CoreOperationError>
			doRequest(const api::DataRequest& request);
		cpp::result<api::Response, api::CoreOperationError> run();
		cpp::result<api::Response, api::CoreOperationError> join();
		cpp::result<api::Response, api::CoreOperationError> stop();
		cpp::result<api::Response, api::CoreOperationError> input(const api::request::Input& request
		);
		cpp::result<api::Response, api::CoreOperationError> output();

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
		cpp::result<api::Response, api::CoreOperationError>
			doRequest(const api::RequestVariant& request);

		VCPU(bool use_stdio):
			  status(api::ExecutionNotStarted{}),
			  uses_stdio(use_stdio),
			  serviceManager(*this) {};
		virtual ~VCPU();
	};
}
