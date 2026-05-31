#include <program_lowering_context.hpp>

#include <backends/dvm/dvm_backend.hpp>

#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

namespace compiler::backend_vm {

	DVMCodeBuilder::DVMCodeBuilder(
		query::Context& query_ctx, bool build_debug_info, bool is_comp_time_lowering
	):
		  program_context(makeBox<internal::ProgramLoweringContext>(
			  query_ctx, build_debug_info, is_comp_time_lowering
		  )),
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

	void DVMCodeBuilder::insertLirGlobal(const lir::LIRGlobalData& lir_global) {
		program_context->lowerAndKeepLirGlobal(lir_global);
	}

	void DVMCodeBuilder::insertRawBytecodeDefinitions(const vm::code::CodeCollection& bytecode) {
		program_context->insertRawBytecodeDefinitions(bytecode);
	}
}
