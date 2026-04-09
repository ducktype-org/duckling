#include "program_lowering_context.hpp"

#include <backends/dvm/repl_lowering.hpp>

#include <base/pointers/box.hpp>

namespace compiler::backend_vm {
	/**
	 * @brief Factory function: Create a new persistent lowering context for REPL.
	 *
	 */
	Box<internal::ProgramLoweringContext> createReplLoweringContext(query::Context& query_ctx) {
		return makeBox<internal::ProgramLoweringContext>(query_ctx, false);
	}

	ReplLoweringContext::ReplLoweringContext(query::Context& query_ctx):
		  m_context(createReplLoweringContext(query_ctx)) {}

	ReplLoweringContext::~ReplLoweringContext()                                         = default;
	ReplLoweringContext::ReplLoweringContext(ReplLoweringContext&&) noexcept            = default;
	ReplLoweringContext& ReplLoweringContext::operator=(ReplLoweringContext&&) noexcept = default;

	internal::ProgramLoweringContext& ReplLoweringContext::getContext() { return *m_context; }

	void ReplLoweringContext::setContext(query::Context& query_ctx) {
		m_context->setContext(query_ctx);
	}

	void ReplLoweringContext::invalidateContext() { m_context->invalidateContext(); }

	base::Optional<base::Ref<query::Context>> ReplLoweringContext::getActiveContext() const {
		return m_context->getActiveContext();
	}

	const vm::code::Function& ReplLoweringContext::lowerAndKeepLirFunction(
		CRef<lir::Function> lir_function
	) {
		return m_context->lowerAndKeepLirFunction(lir_function);
	}

	const vm::code::GlobalData& ReplLoweringContext::lowerAndKeepLirGlobal(
		const lir::LIRGlobal&               lir_global,
		base::Optional<CRef<lir::Function>> global_ctor,
		base::Optional<CRef<lir::Function>> global_dtor
	) {
		return m_context->lowerAndKeepLirGlobal(lir_global, global_ctor, global_dtor);
	}

	const vm::code::TypeOfData& ReplLoweringContext::lowerAndKeepTslType(CRef<tsl::TypeLayout> layout
	) {
		return m_context->lowerAndKeepTslType(layout);
	}
}
