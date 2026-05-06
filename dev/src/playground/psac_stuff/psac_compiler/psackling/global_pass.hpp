#include "context.hpp"
#include "frontend/pst_parser/pst_visitor.hpp"
#include "llvm.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/numeric_value.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>


namespace pg {
	class GlobalPass final: public pst::PstVisitorPanicky {
	public:
		GlobalPass(pg::Context& ctx, LlvmProgramModule& llvm_module):
			  ctx(ctx),
			  llvm_module(llvm_module) {}

	private:
		Context&           ctx;
		LlvmProgramModule& llvm_module;

		void visitNamespace(pst::Access<pst::Namespace> stmt) override {
			auto ns_sym_id = ctx.declareSymbol(
				stmt->getName(),
				sym::NamespaceSymbol{
                }
			);
			stmt->getName().strView();
			// stmt->getBody().illegalAccess().value()->acceptVisitor(*this);
		}
	};
