#pragma once

#include "vm/core/fast/program/program.hpp"
#include "vm/core/fast/runtime.hpp"
#include <vm/core/thread/ivmthread.hpp>
#include <vm/utils/vm_not_implemented.hpp>

namespace vm::fast {
	class FastVMProcess;

	class FastVMThread: public vm::IVMThread {
	public:
		FastVMThread(api::ThreadID thread_id, FastVMProcess& process);

		std::expected<api::Response, api::ApiError> getCurrentPosition() override;

		[[nodiscard]] u64 getNumberOfCurrentStackFrames() const override;

	protected:
		void run(const std::string& func_name, const RunArguments& run_arguments) override;

		void executeOneStep() override;

		void execGlobalDestructors() override;

	private:
		FastVMProcess& fast_process;

		ThreadRuntimeData runtime_data;

		exec::ExecFunctionCollection functions;
		ProgramBase                  program;

		exec::ExecFunction createStartFunctionFor(
			const exec::ExecFunction& function, const FunctionRunArguments& run_arguments
		);

		u64 createStartAndExecuteFunction(
			const exec::ExecFunction& function, const FunctionRunArguments& run_arguments
		);
		u64 executeFunction(
			const exec::ExecFunction& function, const FunctionRunArguments& run_arguments
		);
	};
}
