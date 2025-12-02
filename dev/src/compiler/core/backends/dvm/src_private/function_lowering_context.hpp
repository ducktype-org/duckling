#pragma once

#include <lir/lir_structure/lir_structure.hpp>

#include "base/except/exceptions.hpp"
#include <base/pointers/ref.hpp>

namespace compiler::backend_vm::internal {
	class ProgramLoweringContext;

	class FunctionLoweringContext {
	public:
		const std::string& getBlockLabel(lir::BlockRef block) {
			throw base::NotYetImplemented("getBlockLabel");
		}

	private:
		friend class ProgramLoweringContext;

		FunctionLoweringContext(
			ProgramLoweringContext& program_context, CRef<lir::Function> lir_function
		):
			  program_context(program_context),
			  lir_function(lir_function) {}

		ProgramLoweringContext& program_context;
		CRef<lir::Function>     lir_function;
	};
}
