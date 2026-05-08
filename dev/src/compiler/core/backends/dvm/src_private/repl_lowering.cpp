#include "program_lowering_context.hpp"

#include <backends/dvm/repl_lowering.hpp>

#include <base/pointers/box.hpp>

namespace compiler::backend_vm {
	/**
	 * @brief Factory function: Create a new persistent lowering context for REPL.
	 *
	 */
	Box<internal::ProgramLoweringContext> createReplLoweringContext(query::Context& query_ctx) {
		return makeBox<internal::ProgramLoweringContext>(query_ctx, false, false);
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

	usize ReplLoweringContext::getExtraBytecodeFunctionCount() const {
		return m_context->getExtraBytecodeFunctionCount();
	}

	usize ReplLoweringContext::getLoweredTypeCount() const {
		return m_context->getLoweredTypeCount();
	}

	usize ReplLoweringContext::getLoweredFunctionCount() const {
		return m_context->getLoweredFunctionCount();
	}

	LoweredEntitiesSnapshot ReplLoweringContext::captureLoweredEntitiesSnapshot() const {
		return m_context->captureLoweredEntitiesSnapshot();
	}

	std::vector<vm::code::Function> ReplLoweringContext::getExtraBytecodeFunctionsSince(
		usize start_index
	) const {
		return m_context->getExtraBytecodeFunctionsSince(start_index);
	}

	std::vector<vm::code::TypeOfData> ReplLoweringContext::getLoweredTypesSince(usize start_index
	) const {
		return m_context->getLoweredTypesSince(start_index);
	}

	std::vector<vm::code::Function> ReplLoweringContext::getLoweredFunctionsSince(usize start_index
	) const {
		return m_context->getLoweredFunctionsSince(start_index);
	}

	vm::code::CodeCollection ReplLoweringContext::collectNewCodeSince(
		const LoweredEntitiesSnapshot& snapshot
	) const {
		return m_context->collectNewCodeSince(snapshot);
	}
}
