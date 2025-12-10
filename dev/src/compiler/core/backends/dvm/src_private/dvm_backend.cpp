#include "program_lowering_context.hpp"

#include <backends/dvm/dvm_backend.hpp>

#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

#include "vm/bytecode/bytecode.hpp"

namespace compiler::backend_vm {

	Module::Module(base::StrID module_id):
		  module_id(module_id),
		  program_context(makeBox<internal::ProgramLoweringContext>()) {}

	vm::code::CodeCollection Module::build() const {
		match_optional(program_context->validateAndProduceProgram()) {
			opt_some(program) return program;
			opt_err(error) CORE_PANIC("Failed to validate DVM module: ", error);
		}
		CORE_UNREACHABLE();
	}

	void Module::insertLirFunction(CRef<lir::Function> lir_function) {
		program_context->lowerAndKeepLirFunction(lir_function);
	}

	void Module::insertExternCFunction(const vm::code::ExternalCFunction& extern_func) {
		program_context->insertExternCFunction(extern_func);
	}

	void Module::insertLirGlobal(
		const lir::LIRGlobal&               lir_global,
		base::Optional<CRef<lir::Function>> global_ctor,
		base::Optional<CRef<lir::Function>> global_dtor
	) {
		program_context->lowerAndKeepLirGlobal(lir_global, global_ctor, global_dtor);
	}
}
