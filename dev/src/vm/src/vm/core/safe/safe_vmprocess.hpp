#pragma once

#include <base/collections/object_pool.hpp>
#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/status.hpp>
#include <vm/api/settings.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/process/vmprocess.hpp>
#include <vm/core/safe/concurrency/gil.hpp>
#include <vm/core/safe/concurrency/synchronization_primitives.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/safe_vmthread.hpp>
#include <vm/loader/compiler/safe/safe_compiler.hpp>
#include <vm/loader/loader.hpp>

#include <expected>
#include <string>
#include <variant>
#include <vector>

namespace vm {

	/**
	 * @brief A "safe" implementation of the VMProcess interface.
	 *
	 * @note Only execution of the code is done in the separate thread,
	 * loading and parsing of the program is done in the caller's thread.
	 */
	class SafeVMProcess final: public IVMProcess {
		friend class VmValue;
		friend class VMValueRef;
		friend class SafeVMThread;

	private:
		std::shared_mutex rw_global;

		/**
		 * @brief A loader instance for this SafeVMProcess. Stores the high level and low level
		 * representation of the currently executed program. `loaded_program` references the low
		 * representation which exists in this class.
		 */
		loader::Loader                       loader{};
		loader::compiler::safe::SafeCompiler compiler{ *loader.getHighProgram() };

		/**
		 * @brief The program being executed by this process.
		 * Holds a constant and stable reference.
		 */
		CRef<low::ILowVMProgram> loaded_program;

		low::LowVMProgramCopy loaded_program_copy;

		Memory                 memory;
		IMemory<ShadowEntry>   shadow_data_memory;
		IMemory<ShadowPointer> shadow_pointer_memory;

		GIL                       gil;
		SynchronizationPrimitives synchronization_primitives;

		/**
		 * @brief Storage for all VmValues which belong to this process.
		 * @note Lifetime of these VmValues is controlled by this process. They will be destructed
		 * when process is deinitialized.
		 */
		std::vector<Box<VmValue>> owned_vm_values;

		/**
		 * @brief Pool of threads in this process.
		 * @note Thread with ID 0 is the main thread, it is created together with the process.
		 * Not recycling, because SafeVMThread is not move-constructible.
		 */
		base::StableObjectPool<SafeVMThread, api::ThreadID, false, true> vm_threads;

		/**
		 * @brief Returns first thread in thread queue.
		 */
		SafeVMThread& getMainVMThread();

		/**
		 * @brief Returns reference to either existing empty thread or
		 * creates new thread without worker and returns it
		 */
		SafeVMThread& getEmptyThread();

		base::Optional<api::ApiError> assertProcessCanRespond();

		api::ProcessSettings             settings_;
		std::vector<ShadowEntry>         global_shadow_data;
		std::vector<ShadowPointer>       global_shadow_pointer;
		std::vector<ShadowBlock*>        global_shadow_blocks;
		std::vector<ShadowPointerBlock*> global_shadow_pointer_blocks;

	public:
		SafeVMProcess(PID my_pid, const api::ProcessSettings& settings = {});
		~SafeVMProcess() override;

		[[nodiscard]] Ref<ShadowBlock> getGlobalShadowBlock(u64 idx) {
			return { global_shadow_blocks.at(idx) };
		}

		[[nodiscard]] Ref<ShadowPointerBlock> getGlobalShadowPointerBlock(u64 idx) {
			return { global_shadow_pointer_blocks.at(idx) };
		}

		/**
		 * @brief Returns thread by id and if id doesn't exist or it is equal 0
		 * then it returns main thread
		 */
		base::Optional<Ref<SafeVMThread>> getVMThreadByID(api::ThreadID thread_id);

		Memory& getMemory();

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

		std::expected<api::Response, api::StateError> getExitCode() override;

		std::expected<api::Response, api::ApiError> deinitAndValidate() override;

		base::Optional<api::ApiError> pauseVMThread(api::ThreadID thread_id) override;

		base::Optional<api::ApiError> resumeVMThread(api::ThreadID thread_id) override;

		base::Optional<api::ApiError> stepVMThread(api::ThreadID thread_id) override;

		std::expected<api::Response, api::ApiError> getVMThreadCurrentPosition(
			api::ThreadID thread_id
		) override;

		void notifyPausedVMThread(api::ThreadID thread_id) override;

		void waitForBreakpoint() override;

		std::expected<api::Response, api::ApiError> getNumberOfCurrentStackFrames(
			api::ThreadID thread_id
		) override;

		std::expected<api::Response, api::ApiError> getStackFrameData(
			api::ThreadID thread_id, u64 frame_index
		) override;

		std::expected<api::Response, api::ApiError> getTypeMetadata(
			const std::string& type_name
		) override;

		std::expected<api::Response, api::ApiError> getVMValueForType(
			const std::string& type_name
		) override;

		api::ThreadID getMainThreadID() override;

		std::vector<api::ThreadID> getAllThreadIDs() override;

		std::expected<api::Response, api::ApiError> setBreakpoint(
			base::StrID function_name, usize instruction_index, bool enable
		) override;

		std::expected<api::Response, api::ApiError> mapFileLineToCodeCollectionPosition(
			const fs::File& file, usize line_number
		) override;

		/**
		 * @brief Updates the memory for globals of this process after loading a program with new
		 * globals. Works in incremental way. Only supports adding new globals, not removing or
		 * changing existing ones.
		 *
		 * Should be called after loading a new globals.
		 * @param program The program with the new globals.
		 */
		void updateGlobalDataMemory(CRef<low::ILowVMProgram> program);

		Ref<VmValue> createVmValue(TypeCRef type) override;

		Ref<VmValue> createVmValue(TypeCRef type, Pointer src) override;

		Box<VmValue> createOwnedVmValue(TypeCRef type) override;

		Box<VmValue> createOwnedVmValue(TypeCRef type, Pointer src) override;

		CRef<low::ILowVMProgram> getLoadedProgram() const { return loaded_program; }

		const api::ProcessSettings& getSettings() const { return settings_; }

		/**
		 * @brief Get the GIL of the process.
		 */
		GIL& getGIL();

		/**
		 * @brief Get the synchronization primitives of the process.
		 */
		SynchronizationPrimitives& getSynchronizationPrimitives();
	};
}
