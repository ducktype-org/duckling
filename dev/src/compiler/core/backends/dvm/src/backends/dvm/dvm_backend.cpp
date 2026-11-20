// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <backends/dvm/dvm_backend.hpp>
#include <program_lowering_context.hpp>

#include <base/except/exceptions.hpp>
#include <base/pointers/box.hpp>

#include <logger/logger.hpp>

namespace compiler::backend_vm {

	DVMCodeBuilder::DVMCodeBuilder(
		query::Context& query_ctx,
		base::StrID     module_id,
		bool            build_debug_info,
		bool            is_comp_time_lowering
	):
		  program_context(makeBox<internal::ProgramLoweringContext>(
			  query_ctx, module_id, build_debug_info, is_comp_time_lowering
		  )),
		  build_debug_info(build_debug_info) {}

	vm::code::CodeCollection DVMCodeBuilder::build() const {
		return program_context->produceCodeCollection();
	}

	base::Optional<debug_info::DebugInfo> DVMCodeBuilder::buildDebugInfo() {
		if (build_debug_info) return program_context->buildDebugInfo();
		return {};
	}

	void DVMCodeBuilder::insertLIRUnit(const lir::LIRUnit& lir_unit) {
		for (const auto& lir_global: lir_unit.lir_globals) insertLIRGlobal(lir_global);
		for (const auto& lir_function: lir_unit.lir_functions) insertLIRFunction(lir_function);
	}

	void DVMCodeBuilder::insertLIRFunction(CRef<lir::Function> lir_function) {
		if (lir_function->ignore_on_dvm) return;

		program_context->lowerAndKeepLirFunction(lir_function);
	}

	void DVMCodeBuilder::insertExternCFunction(const vm::code::ExternalCFunction& extern_func) {
		program_context->insertExternCFunction(extern_func);
	}

	void DVMCodeBuilder::insertLIRGlobal(const lir::LIRGlobalData& lir_global) {
		program_context->lowerAndKeepLirGlobal(lir_global);
	}

	void DVMCodeBuilder::insertRawBytecodeDefinitions(const vm::code::CodeCollection& bytecode) {
		program_context->insertRawBytecodeDefinitions(bytecode);
	}
}
