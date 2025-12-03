#include "program_lowering_context.hpp"

#include <backends/dvm/backend.hpp>

#include <base/pointers/box.hpp>

namespace compiler::backend_vm {
	Module::Module(
		query::Context&                         query_ctx,
		base::StrID                             module_id,
		const std::vector<CRef<lir::Function>>& functions,
		const std::vector<BackendDVMGlobal>&    globals
	):
		  module_id(module_id),
		  program_context(makeBox<internal::ProgramLoweringContext>()) {

		// for (const auto& function: functions)
		// 	program_context->forwardDeclareFunction(
		// 		function->mangled_name,
		// 		getDVMSignatureFromLayouts(function->parameter_layouts, function->return_type_layout)
		// 	);

		for (const auto& lir_function: functions) {
			auto func_ctx = program_context->getFuncLoweringCtx(lir_function);
			for (const auto& lir_block: lir_function->block_order) {
				for (const auto& lir_instruction: lir_block->instructions) {
					lir_instruction.output }
			}
		}
	}

	vm::code::CodeCollection Module::build() const {
		auto maybe_program = program_context->validateAndProduceProgram();
		if (!maybe_program.has_value())
			CORE_PANIC("Failed to validate DVM module: ", maybe_program.error());
		else
			return maybe_program.value();
	}
}
