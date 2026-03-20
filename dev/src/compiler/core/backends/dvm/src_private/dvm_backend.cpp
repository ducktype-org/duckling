#include "program_lowering_context.hpp"

#include <backends/dvm/dvm_backend.hpp>

#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

namespace compiler::backend_vm {

	DVMCodeBuilder::DVMCodeBuilder(query::Context& query_ctx, bool build_debug_info):
		  program_context(makeBox<internal::ProgramLoweringContext>(query_ctx, build_debug_info)),
		  build_debug_info(build_debug_info) {}

	vm::code::CodeCollection DVMCodeBuilder::build() const {
		match_optional(program_context->validateAndProduceProgram()) {
			opt_some(program) return program;
			opt_err(error) CORE_PANIC("Failed to validate DVM module: ", error);
		}
		CORE_UNREACHABLE();
	}

	base::Optional<debug_info::DebugInfo> DVMCodeBuilder::buildDebugInfo() {
		CORE_ASSERT(
			build_debug_info,
			"Debug info was not built for this module. To build debug info, construct the "
			"DVMCodeBuilder with build_debug_info=true."
		);
		return program_context->buildDebugInfo();
	}

	void DVMCodeBuilder::insertLirFunction(CRef<lir::Function> lir_function) {
		program_context->lowerAndKeepLirFunction(lir_function);
	}

	void DVMCodeBuilder::insertExternCFunction(const vm::code::ExternalCFunction& extern_func) {
		program_context->insertExternCFunction(extern_func);
	}

	void DVMCodeBuilder::insertLirGlobal(
		const lir::LIRGlobal&               lir_global,
		base::Optional<CRef<lir::Function>> global_ctor,
		base::Optional<CRef<lir::Function>> global_dtor
	) {
		program_context->lowerAndKeepLirGlobal(lir_global, global_ctor, global_dtor);
	}

	void DVMCodeBuilder::insertRawBytecodeDefinitions(const vm::code::CodeCollection& bytecode) {
		program_context->insertRawBytecodeDefinitions(bytecode);
	}
}
