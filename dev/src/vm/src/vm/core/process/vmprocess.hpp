#pragma once

#include "interface_types.hpp"

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/process/concurrency/gil.hpp>
#include <vm/core/process/concurrency/synchronization_primitives.hpp>
#include <vm/core/process/memory/pointer.hpp>
#include <vm/core/process/proc_io.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>

#include <expected>
#include <variant>

namespace vm {

	class VmValue;

	class VMProcess {
	protected:
		PID                              my_pid;
		ProcIO                           io;
		base::Optional<ProcIORedirecter> io_redirecter;
		GIL                              gil;
		SynchronizationPrimitives        synchronization_primitives;

	public:
		VMProcess(PID my_pid);
		virtual ~VMProcess() = default;

		virtual void            setStatus(const api::ProcStatus& new_status) noexcept = 0;
		virtual api::ProcStatus getStatus()                                           = 0;

		ProcIO& getIO();

		/**
		 * @brief Entry point to perform requests on the process.
		 */
		std::expected<api::Response, api::ApiError> doRequest(const api::RequestVariant& request);

		/**
		 * @brief Get the PID of the process.
		 */
		[[nodiscard]] PID getPID() const;

		virtual Ref<VmValue> createVmValue(TypeCRef type)              = 0;
		virtual Ref<VmValue> createVmValue(TypeCRef type, Pointer src) = 0;

		virtual Box<VmValue> createOwnedVmValue(TypeCRef type)              = 0;
		virtual Box<VmValue> createOwnedVmValue(TypeCRef type, Pointer src) = 0;

		GIL&                       getGIL();
		SynchronizationPrimitives& getSynchronizationPrimitives();

		virtual std::expected<api::Response, api::LoadProgramError> loadProgram(
			const std::variant<std::vector<fs::File>, code::CodeCollection>& source
		) = 0;
		virtual std::expected<api::Response, api::ApiError> runFunction(
			const std::string& func_name, const RunArguments& run_arguments
		) = 0;
		virtual std::expected<api::Response, api::ApiError> runFunctionAwait(
			const std::string& func_name, const RunArguments& run_arguments
		)                                                                                 = 0;
		virtual std::expected<api::Response, api::ApiError> join(api::ThreadID thread_id) = 0;
		virtual std::expected<api::Response, api::ApiError> stop()                        = 0;
		virtual std::expected<api::Response, api::ApiError> input(const api::request::Input& request
		)                                                                                 = 0;
		virtual std::expected<api::Response, api::ApiError> output()                      = 0;

		virtual std::expected<api::Response, api::StateError> getExitCode()       = 0;
		virtual std::expected<api::Response, api::ApiError>   deinitAndValidate() = 0;

		virtual std::expected<api::Response, api::ApiError> attach(
			std::istream& istream, std::ostream& ostream
		);
		virtual std::expected<api::Response, api::ApiError> detach();

		// Virtual thread dependencies for doRequest
		virtual base::Optional<api::ApiError> pauseVMThread(api::ThreadID thread_id)  = 0;
		virtual base::Optional<api::ApiError> resumeVMThread(api::ThreadID thread_id) = 0;
		virtual base::Optional<api::ApiError> stepMainVMThread()                      = 0;
		virtual std::expected<api::Response, api::ApiError> getVMThreadCurrentPosition(
			api::ThreadID thread_id
		)                                                                                    = 0;
		virtual std::expected<api::Response, api::ApiError> getMainVMThreadCurrentPosition() = 0;

		virtual void waitForBreakpoint() = 0;

		virtual std::expected<api::Response, api::ApiError> getTypeMetadata(
			const std::string& type_name
		) = 0;
	};
}
