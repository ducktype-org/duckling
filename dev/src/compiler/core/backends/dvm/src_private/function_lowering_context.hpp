#pragma once

#include "dvm_value.hpp"

#include <lir/lir_structure/lir_structure.hpp>

#include <base/pointers/ref.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/utils/interpret.hpp>

namespace compiler::backend_vm::internal {
	class ProgramLoweringContext;

	class FunctionLoweringContext {
	public:
		base::StrID getBlockLabel(lir::BlockRef block);

		const DVMLocal& getDVMLocal(lir::LIRLocalRef local);

		DVMValue lowerLirValue(const lir::LIRValue& lir_value);

		ProgramLoweringContext& getProgramContext() { return program_context; }

		// void push

	private:
		friend class ProgramLoweringContext;

		FunctionLoweringContext(
			ProgramLoweringContext& program_context, CRef<lir::Function> lir_function
		);

		const DVMLocal& insertLocalToDVMMapping(lir::LIRLocalRef local);

		void insertParameterLocals();

		ProgramLoweringContext& program_context;
		CRef<lir::Function>     lir_function;

		base::Map<lir::LIRLocalRef, DVMLocal> lir_local_to_dvm;
		base::Map<lir::BlockRef, base::StrID> block_to_label;

		vm::code::Function function;
	};
}
