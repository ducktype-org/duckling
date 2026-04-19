#include "program_lowering_context.hpp"

#include <backends/dvm/dvm_backend.hpp>

#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

namespace compiler::backend_vm {

	DVMCodeBuilder::DVMCodeBuilder(query::Context& query_ctx, bool build_debug_info):
		  program_context(makeBox<internal::ProgramLoweringContext>(query_ctx, build_debug_info)),
		  build_debug_info(build_debug_info) {}

	vm::code::CodeCollection DVMCodeBuilder::build() const {
		return program_context->produceCodeCollection();
	}

	base::Optional<debug_info::DebugInfo> DVMCodeBuilder::buildDebugInfo() {
		if (build_debug_info) return program_context->buildDebugInfo();
		return {};
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
