#include <backends/dvm/repl_lowering.hpp>
#include <program_lowering_context.hpp>

#include <base/pointers/box.hpp>

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

	ReplDVMCodeBuilder::~ReplDVMCodeBuilder()                                         = default;
	ReplDVMCodeBuilder::ReplDVMCodeBuilder(ReplDVMCodeBuilder&&) noexcept            = default;
	ReplDVMCodeBuilder& ReplDVMCodeBuilder::operator=(ReplDVMCodeBuilder&&) noexcept = default;

	void ReplDVMCodeBuilder::setContext(query::Context& query_ctx) {
		m_context->setContext(query_ctx);
	}

	void ReplDVMCodeBuilder::invalidateContext() { m_context->invalidateContext(); }

	base::Optional<base::Ref<query::Context>> ReplDVMCodeBuilder::getActiveContext() const {
		return m_context->getActiveContext();
	}

	// const vm::code::Function& ReplDVMCodeBuilder::lowerAndKeepLirFunction(
	// 	CRef<lir::Function> lir_function
	// ) {
	// 	return m_context->lowerAndKeepLirFunction(lir_function);
	// }

	// const vm::code::GlobalData& ReplDVMCodeBuilder::lowerAndKeepLirGlobal(
	// 	const lir::LIRGlobalData& lir_global
	// ) {
	// 	return m_context->lowerAndKeepLirGlobal(lir_global);
	// }

	vm::code::CodeCollection ReplDVMCodeBuilder::insertLIRUnitAndCollectNewlyLoweredCode(const lir::LIRUnit& lir_unit) {
		// DEAL WITH THIS PR

		// @TODO: #2246 check if we can avoid repeating the logic from compileLirToModuleData.
		// This is strictly connected to the loading dvm context.
		// We mimic the same idea as in compiling a single module,
		// but this time we append the new functions to the lowering context.

		
		
		auto snapshot = m_context->captureLoweredEntitiesSnapshot();

		for (const auto& global: lir_unit.lir_globals)
			(void) m_context->lowerAndKeepLirGlobal(global);

		// Lower all functions
		for (const auto& lir_function: lir_unit.lir_functions) {
			CORE_DEV_LOG(REPL, "Lowering function: ", lir_function->mangled_name.strView(), "\n");
			// PR #2246 
			// @TODO: #2483 Remove this filter (and the helper function) when strings work in DVM.
			if (lirFunctionDealsWithStrings(lir_function)) {
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


	LoweredEntitiesSnapshot ReplDVMCodeBuilder::captureLoweredEntitiesSnapshot() const {
		return m_context->captureLoweredEntitiesSnapshot();
	}

	vm::code::CodeCollection ReplDVMCodeBuilder::collectNewCodeSince(
		const LoweredEntitiesSnapshot& snapshot
	) const {
		return m_context->collectNewCodeSince(snapshot);
	}
}
