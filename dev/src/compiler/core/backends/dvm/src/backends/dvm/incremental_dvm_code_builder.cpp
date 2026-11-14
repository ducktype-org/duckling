// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <backends/dvm/incremental_dvm_code_builder.hpp>
#include <program_lowering_context.hpp>

#include <base/pointers/box.hpp>

#include <logger/logger.hpp>

namespace compiler::backend_vm {

	IncrementalDVMCodeBuilder::IncrementalDVMCodeBuilder(
		query::Context& query_ctx, bool is_comp_time_lowering
	):
		  code_builder(query_ctx, base::StrID("repl"), false, is_comp_time_lowering) {}

	IncrementalDVMCodeBuilder::~IncrementalDVMCodeBuilder() = default;
	IncrementalDVMCodeBuilder::IncrementalDVMCodeBuilder(IncrementalDVMCodeBuilder&&) noexcept
		= default;
	IncrementalDVMCodeBuilder& IncrementalDVMCodeBuilder::operator=(IncrementalDVMCodeBuilder&&) noexcept
		= default;

	void IncrementalDVMCodeBuilder::setContext(query::Context& query_ctx) {
		code_builder.program_context->setContext(query_ctx);
	}

	void IncrementalDVMCodeBuilder::invalidateContext() {
		code_builder.program_context->invalidateContext();
	}

	base::Optional<base::Ref<query::Context>> IncrementalDVMCodeBuilder::getActiveContext() const {
		return code_builder.program_context->getActiveContext();
	}

	vm::code::CodeCollection IncrementalDVMCodeBuilder::insertLIRUnitAndCollectNewlyLoweredCode(
		const lir::LIRUnit& lir_unit
	) {
		auto snapshot = code_builder.program_context->captureLoweredEntitiesSnapshot();

		CORE_DEV_LOG(
			REPL,
			"Lowering context snapshot: types=",
			snapshot.loweredTypeCount(),
			", globals=",
			snapshot.loweredGlobalCount(),
			", functions=",
			snapshot.loweredFunctionCount(),
			", helper_functions=",
			snapshot.extraBytecodeFunctionCount(),
			"\n"
		);

		code_builder.insertLIRUnit(lir_unit);

		vm::code::CodeCollection new_code
			= code_builder.program_context->collectNewCodeSince(snapshot);
		return new_code;
	}

	void IncrementalDVMCodeBuilder::insertRawBytecodeDefinitions(
		const vm::code::CodeCollection& bytecode
	) {
		code_builder.insertRawBytecodeDefinitions(bytecode);
	}
}
