#include "fast_vmthread.hpp"

#include "fast_vmprocess.hpp"

#include "vm/core/thread/ivmthread.hpp"
#include "vm/core/thread/kill_process_exception.hpp"


using namespace vm;

vm::fast::FastVMThread::FastVMThread(api::ThreadID thread_id, FastVMProcess& process):
	  IVMThread(thread_id, process),
	  fast_process(process) {}

std::expected<api::Response, api::ApiError> vm::fast::FastVMThread::getCurrentPosition() {
	return std::unexpected(
		api::ApiError{
			api::NotImplementedError{ "Method `getCurrentPosition` is not implemented." } }
	);
}

[[nodiscard]] u64 vm::fast::FastVMThread::getNumberOfCurrentStackFrames() const {
	throw vm::VMNotImplemented("Method `getNumberOfCurrentStackFrames` is not implemented.");
}

void vm::fast::FastVMThread::run(const std::string& func_name, const RunArguments& run_arguments) {
	respondExecutionRequest(api::Running{});

	for (const auto& [global, id, name]: process->getGlobals().allData() | std::views::drop(fast_process.getNextUninitializedGlobalId())) {
		auto block_ref = Ref(runtime_data.global_block_ref_buffer_base[global->global_block_idx]);
		// Insert the global data if it hasn't been initialized; then run constructor if present
		if (not process_memory.isGlobalInitialized(block_ref) && global->ctor_name.has_value()) {
			try {
				const auto& func
					= *process_program->getFunctions().atMaybe(global->ctor_name.value()).value();
				low::LowFuncData start_function = createStartFunctionFor(func, {});
				executeFunction(start_function, func);
				process_memory.setGlobalInitialized(block_ref);
			} catch (const KillProcessException& e) {
				respondExecutionRequest(api::ExecutionPanicked{ e.what() });
			}
		}
	}

	try {
		const auto& maybe_func
			= process_program->getFunctions().atMaybe(base::StrID(func_name.data()));
		if (!maybe_func.has_value()) {
			respondExecutionRequest(
				api::ExecutionPanicked{
					base::strConcat("Called function '", func_name, "' does not exist.") }
			);
			return;
		}
		const auto& func = *maybe_func.value();

		low::LowFuncData start_function = [&]() {
			variant_match(run_arguments) {
				variant_case(ProgramRunArguments, program_run_arguments) {
					return createProgramStartFunction(func, program_run_arguments);
				}
				variant_case(FunctionRunArguments, function_run_data) {
					return createStartFunctionFor(func, function_run_data);
				}
			}
			CORE_UNREACHABLE();
		}();

		const auto exit_value = executeFunction(start_function, func);
		respondExecutionRequest(api::ExecutionCompleted{ exit_value });
	} catch (const vm::KillProcessException& e) {
		respondExecutionRequest(api::ExecutionPanicked{ e.what() });
	}
}

void vm::fast::FastVMThread::executeOneStep() {
	throw vm::VMNotImplemented("Method `executeOneStep` is not implemented.");
}

void vm::fast::FastVMThread::execGlobalDestructors() {
	// TODO: Implement this pure virtual method.
	throw vm::VMNotImplemented("Method `execGlobalDestructors` is not implemented.");
}

u64 vm::fast::FastVMThread::createStartAndExecuteFunction(
	const exec::ExecFunction& function, const FunctionRunArguments& run_arguments
) {
    exec::ExecFunction start_function = createStartFunctionFor(function, run_arguments);
    return executeFunction(start_function, run_arguments);
}

u64 vm::fast::FastVMThread::executeFunction(
	const exec::ExecFunction& function, const FunctionRunArguments& run_arguments
) {
	// TODO: Implement this method.
	throw vm::VMNotImplemented("Method `executeFunction` is not implemented.");
}
