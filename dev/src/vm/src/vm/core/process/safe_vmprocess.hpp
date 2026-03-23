#pragma once

#include "interface_types.hpp"
#include "vmprocess.hpp"

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/vmthread.hpp>
#include <vm/loader/loader.hpp>

#include <condition_variable>
#include <deque>
#include <expected>
#include <shared_mutex>
#include <string>
#include <variant>
#include <vector>

namespace vm {

	/**
	 * @brief The API for using the virtual process of the VM.
	 * It manages process's data, loader and threads.
	 *
	 * SafeVMProcess is an abstract concept that represents the program's execution environment.
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
	class SafeVMProcess final: public VMProcess {
		friend class VmValue;

	private:
		std::shared_mutex rw_global;

		api::ProcStatus             status;
		std::shared_mutex           rw_status;
		std::condition_variable_any status_cv;

		// See: https://en.cppreference.com/w/cpp/io/ios_base/Init
		std::ios_base::Init cin_cout_init;

		/**
		 * @brief A loader instance for this SafeVMProcess. Stores the high level and low level
		 * representation of the currently executed program. `loaded_program` references the low
		 * representation which exists in this class.
		 */
		loader::Loader loader{};

		/**
		 * @brief The program being executed by this process.
		 * Holds a constant reference to the LowVMProgram stored in the processes compiler module.
		 */
		CRef<low::LowVMProgram> loaded_program;

		Memory memory;

		/**
		 * @brief Storage for all VmValues which belong to this process.
		 * @note Lifetime of these VmValues is controlled by this process. They will be destructed
		 * when process is deinitialized.
		 */
		std::vector<Box<VmValue>> owned_vm_values;

		// @TODO: Improve this....
		std::deque<VMThread> vm_threads;

		/**
		 * @brief Returns first thread in thread queue.
		 */
		VMThread& getMainVMThread();

		/**
		 * @brief Returns thread by id and if id doesn't exist or it is equal 0
		 * then it returns main thread
		 */
		VMThread& getVMThreadByID(api::ThreadID thread_id);

		/**
		 * @brief Returns reference to either existing empty thread or
		 * creates new thread without worker and returns it
		 */
		VMThread& getEmptyThread();

		base::Optional<api::ApiError> assertProcessCanRespond();

	public:
		SafeVMProcess(PID my_pid);

		void            setStatus(const api::ProcStatus& new_status) noexcept override;
		api::ProcStatus getStatus() override;

		Memory& getMemory();

		Ref<VmValue> createVmValue(TypeCRef type) override;
		Ref<VmValue> createVmValue(TypeCRef type, Pointer src) override;

		Box<VmValue> createOwnedVmValue(TypeCRef type) override;
		Box<VmValue> createOwnedVmValue(TypeCRef type, Pointer src) override;

		std::expected<api::Response, api::LoadProgramError> loadProgram(
			const std::variant<std::vector<fs::File>, code::CodeCollection>& source
		) override;
		std::expected<api::Response, api::ApiError> runFunction(
			const std::string& func_name, const RunArguments& run_arguments
		) override;
		std::expected<api::Response, api::ApiError> runFunctionAwait(
			const std::string& func_name, const RunArguments& run_arguments
		) override;
		std::expected<api::Response, api::ApiError>   join(api::ThreadID thread_id) override;
		std::expected<api::Response, api::ApiError>   stop() override;
		std::expected<api::Response, api::ApiError>   input(const api::request::Input& request
		  ) override;
		std::expected<api::Response, api::ApiError>   output() override;
		std::expected<api::Response, api::StateError> getExitCode() override;
		std::expected<api::Response, api::ApiError>   deinitAndValidate() override;

		base::Optional<api::ApiError> pauseVMThread(api::ThreadID thread_id) override;
		base::Optional<api::ApiError> resumeVMThread(api::ThreadID thread_id) override;
		base::Optional<api::ApiError> stepMainVMThread() override;
		std::expected<api::Response, api::ApiError> getVMThreadCurrentPosition(api::ThreadID thread_id
		) override;
		std::expected<api::Response, api::ApiError> getMainVMThreadCurrentPosition() override;
		void                                        waitForBreakpoint() override;
		std::expected<api::Response, api::ApiError> getTypeMetadata(const std::string& type_name
		) override;
		std::expected<api::Response, api::ApiError> getVMValueForType(const std::string& type_name
		) override;
	};
}
