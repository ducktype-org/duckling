#include "safe_vmprocess.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/process/exceptions.hpp>
#include <vm/core/thread/low_program/instruction.hpp>
#include <vm/core/thread/low_program/low_program.hpp>
#include <vm/core/thread/safe_vmthread.hpp>
#include <vm/core/thread/vmvalue.hpp>
#include <vm/loader/loader.hpp>
#include <vm/loader/logger.hpp>

#include <expected>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <variant>

namespace vm {
	Memory& SafeVMProcess::getMemory() { return memory; }

	std::expected<api::Response, api::LoadProgramError> SafeVMProcess::loadProgram(
		const std::variant<std::vector<fs::File>, code::CodeCollection>& source
	) {
		std::unique_lock                          lock(rw_global);
		std::expected<void, loader::LoaderLogger> code_result = [&] {
			variant_match(source) {
				variant_case(std::vector<fs::File>, files) { return loader.loadAndCompile(files); }
				variant_case(code::CodeCollection, code) { return loader.loadAndCompile(code); }
			}
			CORE_UNREACHABLE();
		}();

		if (code_result.has_value()) {
			loaded_program_copy.selfUpdate();
			return api::Response(api::response::Empty());
		} else {
			std::stringstream ss;
			code_result.error().dump(ss);
			return std::unexpected(api::LoadProgramError{ ss.str() });
		}
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::runFunction(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::unique_lock lock(rw_global);
		SafeVMThread&    thread   = getEmptyThread();
		bool             response = thread.spawnThreadAndRun(func_name, run_arguments);
		// Setting thread ctx necessary for now, until function pointers implemented
		thread.setThreadCtx("");

		if (!response) return std::unexpected(api::ApiError{ api::RunError{} });
		return api::Response(thread.getThreadID());
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::runFunctionAwait(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::unique_lock lock(rw_global);

		getMainVMThread().runNoSpawn(func_name, run_arguments);
		variant_match(getStatus()) {
			variant_case(api::ExecutionCompleted, completed) { return completed.exit_value; }
			variant_default return std::unexpected(api::StateError(
				hasExecutionStarted(getStatus()) ? "Execution did not complete"
												 : "Execution did not start"
			));
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::join(api::ThreadID thread_id) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread)
			return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
		return opt_thread.value()->join();
	}

	void SafeVMProcess::notifyPausedVMThread(api::ThreadID thread_id) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (opt_thread) opt_thread.value()->notifyPaused();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::stop() {
		for (auto& thread: vm_threads) {
			auto response = thread.stop();

			if (!thread.joinExecutionThread())
				return std::unexpected(api::ApiError{ api::JoinError{} });

			// @TODO: #1222 make two different "stop" functions, one that throws error if
			// program was not stopped successfully and another that does nothing
			if (!response) return std::unexpected(api::OtherError{ "unexpected status response" });
		}

		return api::response::Empty{};
	}

	base::Optional<api::ApiError> SafeVMProcess::assertProcessCanRespond() {
		api::ProcStatus status = getStatus();

		if (!api::canRespond(status))
			return api::ApiError{ api::OtherError{
				"Cannot do memory request while program is running" } };

		return {};
	}

	Ref<VmValue> SafeVMProcess::createVmValue(TypeCRef type) {
		auto value = Box<VmValue>::fromPointer(new VmValue(*this, type));
		owned_vm_values.push_back(std::move(value));
		return owned_vm_values.back().refMut();
	}

	Ref<VmValue> SafeVMProcess::createVmValue(TypeCRef type, Pointer src) {
		auto value = Box<VmValue>::fromPointer(new VmValue(*this, type, src));
		owned_vm_values.push_back(std::move(value));
		return owned_vm_values.back().refMut();
	}

	Box<VmValue> SafeVMProcess::createOwnedVmValue(TypeCRef type) {
		return Box<VmValue>::fromPointer(new VmValue(*this, type));
	}

	Box<VmValue> SafeVMProcess::createOwnedVmValue(TypeCRef type, Pointer src) {
		return Box<VmValue>::fromPointer(new VmValue(*this, type, src));
	}

	SafeVMProcess::SafeVMProcess(const PID my_pid):
		  IVMProcess(my_pid),
		  loaded_program(&loaded_program_copy),
		  loaded_program_copy(loader.getProgram()) {
		vm_threads.add(*this);
	}

	SafeVMThread& SafeVMProcess::getMainVMThread() { return *vm_threads.get(api::ThreadID{ 0 }); }

	base::Optional<Ref<SafeVMThread>> SafeVMProcess::getVMThreadByID(api::ThreadID thread_id) {
		if_opt_some(vm_threads.maybeGet(thread_id), thread) return thread;
		return std::nullopt;
	}

	SafeVMThread& SafeVMProcess::getEmptyThread() {
		for (auto& thread: vm_threads)
			if (!api::isExecuting(thread.getStatus())) return thread;

		return *vm_threads.get(vm_threads.add(*this));
	}

	std::expected<api::Response, api::StateError> SafeVMProcess::getExitCode() {
		std::unique_lock lock(rw_global);
		variant_match(getStatus()) {
			variant_case(api::ExecutionCompleted, completed) { return completed.exit_value; }
			variant_default return std::unexpected(api::StateError(
				hasExecutionStarted(getStatus()) ? "Execution did not complete"
												 : "Execution did not start"
			));
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::deinitAndValidate() {
		std::unique_lock lock(rw_global);
		for (auto& t: vm_threads)
			if (api::isExecuting(t.getStatus()))
				if (auto res = stop(); !res.has_value()) return res;

		for (auto& t: vm_threads) t.joinExecutionThread();

		try {
			// There might be numerous runtime exceptions during the deinitialization,
			// any of those means there was an issue during the validation.
			getMainVMThread().execGlobalDestructors();

			for (const auto& vm_value: owned_vm_values) vm_value->freeData();

			memory.deinitGlobals();
		} catch (exceptions::VMRuntimeException& e) {
			std::cerr << " - VM has detected issues during program\'s deinitialization: "
					  << e.what() << '\n';
			return false;
		}
		return memory.validateMemoryState();
	}

	base::Optional<api::ApiError> SafeVMProcess::pauseVMThread(api::ThreadID thread_id) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread) return api::ApiError{ api::OtherError{ "Thread not found" } };
		auto response = opt_thread.value()->pause();
		if (!response) return api::ApiError{ api::PauseError{} };
		return {};
	}

	base::Optional<api::ApiError> SafeVMProcess::resumeVMThread(api::ThreadID thread_id) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread) return api::ApiError{ api::OtherError{ "Thread not found" } };
		auto response = opt_thread.value()->resume();
		if (!response) return api::ApiError{ api::ResumeError{} };
		return {};
	}

	base::Optional<api::ApiError> SafeVMProcess::stepVMThread(api::ThreadID thread_id) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread) return api::ApiError{ api::OtherError{ "Thread not found" } };
		auto response = opt_thread.value()->step();
		if (!response) return api::ApiError{ api::OtherError{ "step error" } };
		return {};
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getVMThreadCurrentPosition(
		api::ThreadID thread_id
	) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread)
			return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
		return opt_thread.value()->getCurrentPosition();
	}

	void SafeVMProcess::waitForBreakpoint() {
		std::shared_lock lock(rw_status);
		status_cv.wait(lock, [&] {
			return std::holds_alternative<api::Paused>(status) || api::isStatusTerminal(status);
		});
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getNumberOfCurrentStackFrames(
		api::ThreadID thread_id
	) {
		std::shared_lock lock(rw_global);
		match_optional(assertProcessCanRespond()) {
			opt_some(error) { return std::unexpected(error); }
			opt_none {
				auto opt_thread = getVMThreadByID(thread_id);
				if (!opt_thread)
					return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
				u64 frames = opt_thread.value()->getNumberOfCurrentStackFrames();
				return api::Response(api::response::NumberOfCurrentStackFrames{
					.number_of_stack_frames = frames });
			}
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getStackFrameData(
		api::ThreadID thread_id, u64 frame_index
	) {
		std::shared_lock lock(rw_global);
		match_optional(assertProcessCanRespond()) {
			opt_some(error) { return std::unexpected(error); }
			opt_none {
				auto opt_thread = getVMThreadByID(thread_id);
				if (!opt_thread)
					return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
				u64 frames = opt_thread.value()->getNumberOfCurrentStackFrames();
				if (frame_index >= frames)
					return std::unexpected(api::ApiError{
						api::OtherError{ "Frame index out of bounds" } });
				Frame& frame = opt_thread.value()->getStackFrame(frame_index);

				std::vector<api::response::StackFrameData::FrameVar> frame_vars;
				for (Block* block_ptr:
				     std::span(frame.local_block_ref_stack_base, frame.local_block_ref_stack_end)) {
					Ref<Block> block  = Ref(block_ptr);
					u64        offset = base::safeIntConv<u64>(
                        memory.getBlockViewUnsafe(block).getBegin() - frame.local_stack
                    );
					frame_vars.push_back(api::response::StackFrameData::FrameVar{
						.offset = offset,
						.value  = VMValueRef(*this, memory.getBlockType(block), Pointer(block, 0)),
					});
				}

				return api::Response(api::response::StackFrameData{
					.function_name = frame.current_function->name, .frame_vars = frame_vars });
			}
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getTypeMetadata(
		const std::string& type_name
	) {
		std::shared_lock lock(rw_global);
		match_optional(assertProcessCanRespond()) {
			opt_some(error) { return std::unexpected(error); }
			opt_none {
				auto res = loaded_program->getTypes().atMaybe(base::StrID(type_name.c_str()));
				match_optional(res) {
					opt_some(value) { return api::response::Type{ value }; }
					opt_none {
						return std::unexpected(api::ApiError{ api::OtherError{ "Type not found" } });
					}
				}
			}
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> SafeVMProcess::getVMValueForType(
		const std::string& type_name
	) {
		std::shared_lock lock(rw_global);
		match_optional(assertProcessCanRespond()) {
			opt_some(error) { return std::unexpected(error); }
			opt_none {
				auto maybe_type
					= loaded_program->getTypes().atMaybe(base::StrID(type_name.c_str()));
				match_optional(maybe_type) {
					opt_some(type) {
						auto vm_value = createOwnedVmValue(type);
						return api::response::VmValue{ std::move(vm_value) };
					}
					opt_none {
						return std::unexpected(api::ApiError{ api::OtherError{ "Type not found" } });
					}
				}
			}
		}
		CORE_UNREACHABLE();
	}

	std::vector<api::ThreadID> SafeVMProcess::getAllThreadIDs() {
		std::shared_lock           lock(rw_global);
		std::vector<api::ThreadID> thread_ids;
		for (const auto& thread: vm_threads)
			if (api::isExecuting(thread.getStatus())) thread_ids.push_back(thread.getThreadID());
		return thread_ids;
	}

	api::ThreadID SafeVMProcess::getMainThreadID() {
		std::shared_lock lock(rw_global);
		return getMainVMThread().getThreadID();
	}
}
