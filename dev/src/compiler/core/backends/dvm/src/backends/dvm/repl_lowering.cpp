#include <backends/dvm/repl_lowering.hpp>
#include <program_lowering_context.hpp>

#include <base/pointers/box.hpp>

#include <logger/logger.hpp>

namespace compiler::backend_vm {
	/**
	 * @brief Factory function: Create a new persistent lowering context for REPL.
	 *
	 */
	Box<internal::ProgramLoweringContext> createReplLoweringContext(query::Context& query_ctx) {
		return makeBox<internal::ProgramLoweringContext>(query_ctx, false, false);
	}

	ReplDVMCodeBuilder::ReplDVMCodeBuilder(query::Context& query_ctx):
		  m_context(createReplLoweringContext(query_ctx)) {}

	ReplDVMCodeBuilder::~ReplDVMCodeBuilder()                                        = default;
	ReplDVMCodeBuilder::ReplDVMCodeBuilder(ReplDVMCodeBuilder&&) noexcept            = default;
	ReplDVMCodeBuilder& ReplDVMCodeBuilder::operator=(ReplDVMCodeBuilder&&) noexcept = default;

	void ReplDVMCodeBuilder::setContext(query::Context& query_ctx) {
		m_context->setContext(query_ctx);
	}

	void ReplDVMCodeBuilder::invalidateContext() { m_context->invalidateContext(); }

	base::Optional<base::Ref<query::Context>> ReplDVMCodeBuilder::getActiveContext() const {
		return m_context->getActiveContext();
	}

	vm::code::CodeCollection ReplDVMCodeBuilder::insertLIRUnitAndCollectNewlyLoweredCode(
		const lir::LIRUnit& lir_unit
	) {
		// @TODO: #2246 check if we can avoid repeating the logic from DVMCodeBuilder::insertLIRUnit.
		// This is strictly connected to the loading dvm context.
		// We mimic the same idea as in compiling a single module,
		// but this time we append the new functions to the lowering context.

		auto lir_function_deals_with_strings = [](const CRef<lir::Function> lir_function) {
			auto is_string_layout = [](const CRef<tsl::TypeLayout> layout) -> bool {
				return layout->getSourceType().getType().getKind() == tsh::Kind::String;
			};
			if (is_string_layout(lir_function->return_type_layout)) return true;
			for (const auto& param_layout: lir_function->parameter_layouts)
				if (is_string_layout(param_layout)) return true;
			return false;
		};


		auto snapshot = m_context->captureLoweredEntitiesSnapshot();

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

		for (const auto& global: lir_unit.lir_globals)
			(void) m_context->lowerAndKeepLirGlobal(global);

		// Lower all functions
		for (const auto& lir_function: lir_unit.lir_functions) {
			CORE_DEV_LOG(REPL, "Lowering function: ", lir_function->mangled_name.strView(), "\n");
			// @TODO: #2483 Remove this filter (and the helper function) when strings work in DVM.
			if (lir_function_deals_with_strings(lir_function)) {
				CORE_DEV_LOG(
					REPL,
					"Lowering function that deals with strings skipped: ",
					lir_function->mangled_name,
					"\n"
				);
				continue;
			}
			(void) m_context->lowerAndKeepLirFunction(lir_function);
		}

		vm::code::CodeCollection new_code = m_context->collectNewCodeSince(snapshot);
		return new_code;
	}
}
