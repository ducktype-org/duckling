// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <vm/core/fast/program/instructions/executable.hpp>
#include <vm/core/fast/program/program.hpp>
#include <vm/core/fast/runtime.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/thread/ivmthread.hpp>
#include <vm/utils/vm_not_implemented.hpp>

namespace vm::fast {
	class FastVMProcess;
	class FastExecutor;

	class FastVMThread: public vm::IVMThread {
		friend class FastExecutor;
		friend class FastVMProcess;

	public:
		FastVMThread(
			api::ThreadID                      thread_id,
			FastVMProcess&                     process,
			CRef<ProgramBase>                  program,
			CRef<exec::ExecFunctionCollection> functions
		);

		[[nodiscard]] u64 getNumberOfCurrentStackFrames() const override;

	protected:
		void run(const std::string& func_name, const RunArguments& run_arguments) override;

		void executeOneStep() override;

		[[nodiscard]] bool isAtExecutionEnd() const override;

		void execGlobalDestructors() override;

	private:
		i64 exit_value = 0;

		FastVMProcess& fast_process;

		ThreadRuntimeState runtime_data;

		CRef<exec::ExecFunctionCollection> functions;
		CRef<ProgramBase>                  program;

		exec::ExecFunction createStartFunctionFor(
			const exec::ExecFunction& function, const RunArguments& run_arguments
		);

		i64 createStartAndExecuteFunction(
			const exec::ExecFunction& function, const RunArguments& run_arguments
		);
		i64 executeFunction(const exec::ExecFunction& function, const RunArguments& run_arguments);
	};
}
