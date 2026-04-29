#pragma once

#include <vm/core/process/vmprocess.hpp>
#include <vm/utils/vm_not_implemented.hpp>

namespace vm::fast {
	class FastVMProcess final: public IVMProcess {
	public:
		explicit FastVMProcess(PID pid);
		~FastVMProcess() override = default;

		Ref<VmValue> createVmValue(TypeCRef type) override;

		Ref<VmValue> createVmValue(TypeCRef type, Pointer src) override;

		Box<VmValue> createOwnedVmValue(TypeCRef type) override;

		Box<VmValue> createOwnedVmValue(TypeCRef type, Pointer src) override;

		std::expected<api::Response, api::ApiError> doRequest(const api::RequestVariant& request
		) override;


	private:
		std::expected<api::Response, api::LoadProgramError> loadProgram(
			const std::variant<std::vector<fs::File>, code::CodeCollection>& source
		) override;

		std::expected<api::Response, api::ApiError> runFunction(
			const std::string& func_name, const RunArguments& run_arguments
		) override;

		std::expected<api::Response, api::ApiError> runFunctionAwait(
			const std::string& func_name, const RunArguments& run_arguments
		) override;

		std::expected<api::Response, api::ApiError> join(api::ThreadID thread_id) override;

		std::expected<api::Response, api::ApiError> stop() override;


	protected:
		std::expected<api::Response, api::StateError> getExitCode() override;

		std::expected<api::Response, api::ApiError> deinitAndValidate() override;

		base::Optional<api::ApiError> pauseVMThread(api::ThreadID thread_id) override;

		base::Optional<api::ApiError> resumeVMThread(api::ThreadID thread_id) override;

		base::Optional<api::ApiError> stepVMThread(api::ThreadID thread_id) override;

		std::expected<api::Response, api::ApiError> getVMThreadCurrentPosition(api::ThreadID thread_id
		) override;

		std::expected<api::Response, api::ApiError> getNumberOfCurrentStackFrames(
			api::ThreadID thread_id
		) override;

		std::expected<api::Response, api::ApiError> getStackFrameData(
			api::ThreadID thread_id, u64 frame_index
		) override;

		void notifyPausedVMThread(api::ThreadID thread_id) override;

		void waitForBreakpoint() override;

		std::expected<api::Response, api::ApiError> getTypeMetadata(const std::string& type_name
		) override;

		std::expected<api::Response, api::ApiError> getVMValueForType(const std::string& type_name
		) override;

		std::vector<api::ThreadID> getAllThreadIDs() override;

		api::ThreadID getMainThreadID() override;
	};
}
