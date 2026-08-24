#include <backends/dvm/repl_lowering.hpp>
#include <program_lowering_context.hpp>

#include <base/pointers/box.hpp>

#include <logger/logger.hpp>

namespace compiler::backend_vm {

	ReplDVMCodeBuilder::ReplDVMCodeBuilder(query::Context& query_ctx, bool is_comp_time_lowering):
		  code_builder(query_ctx, base::StrID("repl"), false, is_comp_time_lowering) {}

	ReplDVMCodeBuilder::~ReplDVMCodeBuilder()                                        = default;
	ReplDVMCodeBuilder::ReplDVMCodeBuilder(ReplDVMCodeBuilder&&) noexcept            = default;
	ReplDVMCodeBuilder& ReplDVMCodeBuilder::operator=(ReplDVMCodeBuilder&&) noexcept = default;

	void ReplDVMCodeBuilder::setContext(query::Context& query_ctx) {
		code_builder.program_context->setContext(query_ctx);
	}

	void ReplDVMCodeBuilder::invalidateContext() {
		code_builder.program_context->invalidateContext();
	}

	base::Optional<base::Ref<query::Context>> ReplDVMCodeBuilder::getActiveContext() const {
		return code_builder.program_context->getActiveContext();
	}

	vm::code::CodeCollection ReplDVMCodeBuilder::insertLIRUnitAndCollectNewlyLoweredCode(
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

	void ReplDVMCodeBuilder::insertRawBytecodeDefinitions(const vm::code::CodeCollection& bytecode) {
		code_builder.insertRawBytecodeDefinitions(bytecode);
	}
}
