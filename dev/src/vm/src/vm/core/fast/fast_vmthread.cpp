#include "fast_vmthread.hpp"

#include "fast_vmprocess.hpp"

#include <base/except/exceptions.hpp>

#include <vm/core/fast/eval/evaluator.hpp>
#include <vm/core/fast/program/instructions/executable.hpp>
#include <vm/core/fast/program/program.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/thread/ivmthread.hpp>
#include <vm/core/thread/kill_process_exception.hpp>
#include <vm/utils/vm_not_implemented.hpp>


using namespace vm;

vm::fast::FastVMThread::FastVMThread(
	api::ThreadID                      thread_id,
	FastVMProcess&                     process,
	CRef<ProgramBase>                  program,
	CRef<exec::ExecFunctionCollection> functions
):
	  IVMThread(thread_id, process),
	  fast_process(process),
	  functions(functions),
	  program(program) {}

[[nodiscard]] u64 vm::fast::FastVMThread::getNumberOfCurrentStackFrames() const {
	throw vm::VMNotImplemented("Method `getNumberOfCurrentStackFrames` is not implemented.");
}

void vm::fast::FastVMThread::run(const std::string& func_name, const RunArguments& args) {
	// The spawner already committed `Spawn` (see `IVMThread::prepareSpawnLocked`).
	if (!program->functions.contains(base::StrID(func_name.data())))
		throw exceptions::VMRuntimeException(
			base::strConcat("Called function '", func_name, "' does not exist.")
		);

	CRef<FunctionInfo>        func_info = program->functions.at(base::StrID(func_name.data()));
	const exec::ExecFunction& func      = functions->at(func_info->id.asInt());

	exit_value = createStartAndExecuteFunction(func, args);
	applyEvent(thread_sm::thread_event::Finish{ exit_value });
}

void vm::fast::FastVMThread::executeOneStep() {
	throw vm::VMNotImplemented("Method `executeOneStep` is not implemented.");
}

bool vm::fast::FastVMThread::execGlobalDestructors() {
	throw vm::VMNotImplemented("Method `execGlobalDestructors` is not implemented.");
}

[[nodiscard]] i64 vm::fast::FastVMThread::getExitValue() const { return exit_value; }

vm::fast::exec::ExecFunction vm::fast::FastVMThread::createStartFunctionFor(
	const exec::ExecFunction& function, const RunArguments& run_arguments
) {
	// @TODO: #2720 Make use of RunArguments in the start function creation and execution.
	// Keep in mind #2727.
	variant_match(run_arguments) {
		variant_case(ProgramRunArguments, program_args) {
			if (!program_args.empty())
				throw VMNotImplemented(
					"Start function creation for specific RunArguments is not implemented yet."
				);
		}
		variant_case(FunctionRunArguments, function_args) {
			if (!function_args.empty())
				throw VMNotImplemented(
					"Start function creation for specific RunArguments is not implemented yet."
				);
		}
	}

	using namespace vm::fast::exec;
	exec::ExecFunction start_function{};
	usize ret_and_args_size = function.info->return_size.asInt() + function.info->args_size.asInt();
	start_function.data     = {
        maker::init_pany_imm(0, ret_and_args_size),
        maker::call_func_imm(&function, 0),
        maker::exit(),
	};
	return start_function;
}

i64 vm::fast::FastVMThread::createStartAndExecuteFunction(
	const exec::ExecFunction& function, const RunArguments& run_arguments
) {
	exec::ExecFunction start_function = createStartFunctionFor(function, run_arguments);
	return executeFunction(start_function, run_arguments);
}

i64 vm::fast::FastVMThread::executeFunction(const exec::ExecFunction& start_function, const RunArguments&) {
	Frame* frame       = runtime_data.pushFrame(&start_function, runtime_data.local_stack_base);
	byte*  local_stack = runtime_data.local_stack_base;
	FastExecutor::eval(runtime_data, local_stack, frame, *this);
	return *reinterpret_cast<i64*>(runtime_data.local_stack_base);
}
