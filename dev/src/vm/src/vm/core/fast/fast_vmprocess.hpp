#pragma once

#include "fast_vmthread.hpp"

#include <base/collections/object_pool.hpp>

#include <vm/api/data/thread_id.hpp>
#include <vm/core/fast/program/program.hpp>
#include <vm/core/process/ivmprocess.hpp>
#include <vm/loader/compiler/fast/fast_compiler.hpp>
#include <vm/loader/loader.hpp>
#include <vm/utils/vm_not_implemented.hpp>

namespace vm::fast {
	class FastVMProcess final: public IVMProcess {
	public:
		explicit FastVMProcess(PID pid);
		~FastVMProcess() override = default;

		Ref<VmValue> createVmValue(vm::TypeCRef type) override;

		Ref<VmValue> createVmValue(vm::TypeCRef type, Pointer src) override;

		Box<VmValue> createOwnedVmValue(vm::TypeCRef type) override;

		Box<VmValue> createOwnedVmValue(vm::TypeCRef type, Pointer src) override;

		std::expected<api::Response, api::ApiError> doRequest(const api::RequestVariant& request
		) override;

		u64 getNextUninitializedGlobalId() const { return next_to_initialize_global_id; }

		void addInitializedGlobals(u64 count) { next_to_initialize_global_id += count; }

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
		std::expected<api::Response, api::ApiError> mapFileLineToCodeCollectionPosition(
			const fs::File& file, usize line_number
		) override;

		std::expected<api::Response, api::ApiError> setBreakpoint(
			base::StrID function_name, usize instruction_index, bool enable
		) override;

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

		void requestStopAllThreads() noexcept override;

		void waitForBreakpoint() override;

		std::expected<api::Response, api::ApiError> setExecutionConfig(
			const api::ExecutionConfig& config
		) override;

		std::expected<api::Response, api::ApiError> getTypeMetadata(const std::string& type_name
		) override;

		std::expected<api::Response, api::ApiError> getVMValueForType(const std::string& type_name
		) override;

		std::vector<api::ThreadID> getAllThreadIDs() override;

		FastVMThread& getMainVMThread();

		base::Optional<Ref<FastVMThread>> getVMThreadByID(api::ThreadID thread_id);

		api::ThreadID getMainThreadID() override;

	private:
		base::StableObjectPool<FastVMThread, api::ThreadID, false, true> vm_threads;

		u64 next_to_initialize_global_id = 0;

		std::mutex data_lock;

		loader::Loader                       loader;
		loader::compiler::fast::FastCompiler compiler{ *loader.getHighProgram() };

		exec::ExecFunctionCollection functions;
	};
}
