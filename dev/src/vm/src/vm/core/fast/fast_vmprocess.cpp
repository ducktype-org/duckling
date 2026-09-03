#include "fast_vmprocess.hpp"

#include <vm/core/fast/program/relocator.hpp>
#include <vm/core/process/ivmprocess.hpp>
#include <vm/loader/logger.hpp>
#include <vm/utils/vm_not_implemented.hpp>

#include <sstream>

namespace vm::fast {
	FastVMProcess::FastVMProcess(PID pid): IVMProcess(pid) {
		vm_threads.add(*this, compiler.getProgramBase(), &functions);
	}

	Ref<IVMValue> FastVMProcess::createVMValue([[maybe_unused]] code::valid_type::ValidTypeID type_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `createVMValue` is not implemented.");
	}

	Box<IVMValue> FastVMProcess::createOwnedVMValue(
		[[maybe_unused]] code::valid_type::ValidTypeID type_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `createOwnedVMValue` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::doRequest(
		const api::RequestVariant& request
	) {
		// So long api features are not implemented, this method will catch NotImplemented
		// exceptions thrown by them and return them as ApiErrors.
		try {
			return IVMProcess::doRequest(request);
		} catch (const VMNotImplemented& e) {
			return std::unexpected(api::ApiError{ api::NotImplementedError{ e.what() } });
		};
	}

	std::expected<api::Response, api::LoadProgramError> FastVMProcess::loadProgram(
		[[maybe_unused]] const std::variant<std::vector<fs::File>, code::CodeCollection>& source
	) {
		std::scoped_lock                          lock(data_lock);
		std::expected<void, loader::LoaderLogger> code_result = [&] {
			variant_match(source) {
				variant_case(std::vector<fs::File>, files) {
					return loader.loadAndValidate(files, execution_config);
				}
				variant_case(code::CodeCollection, code) {
					return loader.loadAndValidate(code, execution_config);
				}
			}
			CORE_UNREACHABLE();
		}();

		if (code_result.has_value()) {
			compiler.recompile();
			functions = exec::linkFunctions(
				*compiler.getProgramBase(), *compiler.getRelocatableFunctions()
			);
			return api::Response(api::response::Empty());
		} else {
			std::stringstream ss;
			code_result.error().dump(ss);
			return std::unexpected(api::LoadProgramError{ ss.str() });
		}
	}

	std::expected<void, api::ApiError> FastVMProcess::
		validateRunArguments(const std::string&, const RunArguments&) const {
		return {};
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::runFunction(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::unique_lock lock(data_lock);
		FastVMThread&    thread = getMainVMThread();
		// thread.setThreadCtx(func_name);
		bool response = thread.spawnThreadAndRun(func_name, run_arguments);

		if (!response) {
			// thread.setThreadCtx("");
			return std::unexpected(api::ApiError{
				api::RunError{ "Failed to spawn thread for function: " + func_name } });
		}
		return api::Response(thread.getThreadID());
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::runFunctionAwait(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::unique_lock lock(data_lock);

		FastVMThread& thread = getMainVMThread();
		// thread.setThreadCtx(func_name);
		if (!thread.runNoSpawn(func_name, run_arguments)) {
			return std::unexpected(api::ApiError{
				api::RunError{ "Main thread is already executing: " + func_name } });
		}
		// thread.setThreadCtx("");

		const ProcessState state = getProcessState();
		variant_match(state) {
			variant_case(process_state::Completed, completed) { return completed.exit_value; }
			variant_case(process_state::Panicked, panicked) {
				return std::unexpected(api::StateError{ panicked.err });
			}
			variant_default return std::unexpected(api::StateError(
				v_matches(state, process_state::NotStarted) ? "Execution did not start"
															: "Execution did not complete"
			));
		}
		CORE_UNREACHABLE();
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::join(
		[[maybe_unused]] api::ThreadID thread_id
	) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (!opt_thread)
			return std::unexpected(api::ApiError{ api::OtherError{ "Thread not found" } });
		return opt_thread.value()->join();
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::stop() {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `stop` is not implemented.");
	}

	FastVMThread& FastVMProcess::getMainVMThread() { return *vm_threads.get(api::MAIN_THREAD_ID); }

	base::Optional<Ref<FastVMThread>> FastVMProcess::getVMThreadByID(api::ThreadID thread_id) {
		if_opt_some(vm_threads.maybeGet(thread_id), thread) return thread;
		return std::nullopt;
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::mapFileLineToCodeCollectionPosition(
		[[maybe_unused]] const fs::File& file, [[maybe_unused]] usize line_number
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented(
			"Method `mapFileLineToCodeCollectionPosition` is not implemented."
		);
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::setBreakpoint(
		[[maybe_unused]] base::StrID function_name,
		[[maybe_unused]] usize       instruction_index,
		[[maybe_unused]] bool        enable
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `setBreakpoint` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::deinitAndValidate() { return true; }

	std::expected<void, api::ApiError> FastVMProcess::requestPauseOfVMThread(
		[[maybe_unused]] api::ThreadID thread_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `requestPauseOfVMThread` is not implemented.");
	}

	std::expected<void, api::ApiError> FastVMProcess::pauseVMThread(
		[[maybe_unused]] api::ThreadID thread_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `pauseVMThread` is not implemented.");
	}

	std::expected<void, api::ApiError> FastVMProcess::resumeVMThread(
		[[maybe_unused]] api::ThreadID thread_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `resumeVMThread` is not implemented.");
	}

	std::expected<void, api::ApiError> FastVMProcess::stepVMThread(
		[[maybe_unused]] api::ThreadID thread_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `stepVMThread` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::getVMThreadCurrentPosition(
		[[maybe_unused]] api::ThreadID         thread_id,
		[[maybe_unused]] base::Optional<usize> frame_idx = std::nullopt
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getVMThreadCurrentPosition` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::getNumberOfCurrentStackFrames(
		[[maybe_unused]] api::ThreadID thread_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getNumberOfCurrentStackFrames` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::getStackFrameData(
		[[maybe_unused]] api::ThreadID thread_id, [[maybe_unused]] u64 frame_index
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getStackFrameData` is not implemented.");
	}

	void FastVMProcess::notifyVMThreadWaiters([[maybe_unused]] api::ThreadID thread_id) {
		auto opt_thread = getVMThreadByID(thread_id);
		if (opt_thread) opt_thread.value()->notifyWaiters();
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::waitForBreakpointAndReportPosition(
		[[maybe_unused]] api::ThreadID thread_id
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `waitForBreakpointAndReportPosition` is not implemented."
		);
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::setExecutionConfig(
		const api::ExecutionConfig& config
	) {
		std::scoped_lock lock(data_lock);
		execution_config = config;
		return api::Response(api::response::Empty());
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::getTypeMetadata(
		[[maybe_unused]] const std::string& type_name
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getTypeMetadata` is not implemented.");
	}

	std::expected<api::Response, api::ApiError> FastVMProcess::getVMValueForType(
		[[maybe_unused]] const std::string& type_name
	) {
		// @TODO: #2102 Implement this pure virtual method.
		throw vm::VMNotImplemented("Method `getVMValueForType` is not implemented.");
	}

	std::vector<api::ThreadID> FastVMProcess::unjoinedThreadIds() const {
		std::vector<api::ThreadID> ids;
		for (const auto& thread: vm_threads) {
			const thread_state::ThreadState state = thread.getThreadState();
			if (thread_state::hasStarted(state) && !thread_state::isJoined(state))
				ids.push_back(thread.getThreadID());
		}
		return ids;
	}

	std::vector<api::ThreadID> FastVMProcess::getAllActiveThreadIDs() {
		std::vector<api::ThreadID> thread_ids;
		for (const auto& thread: vm_threads)
			if (thread_state::isActive(thread.getThreadState()))
				thread_ids.push_back(thread.getThreadID());
		return thread_ids;
	}

	void FastVMProcess::requestStopAllThreads() noexcept {
		for (auto& thread: vm_threads) thread.requestStop();
	}
}
