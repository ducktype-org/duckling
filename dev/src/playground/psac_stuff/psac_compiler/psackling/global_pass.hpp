#include "context.hpp"
#include "frontend/pst_parser/pst_visitor.hpp"
#include "llvm.hpp"
#include "psackling/ids.hpp"
#include "psackling/type.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/numeric_value.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>

#include "string_id/string_id.hpp"

namespace pg {
	class GlobalPass final: public pst::PstVisitorPanicky {
	public:
		GlobalPass(pg::Context& ctx): ctx(ctx) {}

	private:
		Context& ctx;

		void visitNamespace(pst::Access<pst::Namespace> stmt) override {
			ctx.pushNamespace(stmt->getName());
			GlobalPass nested_pass(ctx);
			for (pst::AccessLocked<pst::LangElement> child: *stmt->getBody().illegalAccess().value())
				child.illegalAccess().value()->acceptVisitor(nested_pass);
			ctx.popNamespace();
		}

		void visitFun(pst::Access<pst::Fun> stmt) override {
			std::vector<tp::TypeID>    param_type_ids;
			std::vector<sym::SymbolID> argument_symbol_ids;
			// We are pushing an empty namespace for the function body
			auto func_ns_id = ctx.pushNamespace(base::StrID(""));
			for (pst::AccessLocked<pst::Param> param: *stmt->getParams().illegalAccess().value()) {
				auto param_accessed = param.illegalAccess().value();
				// param_accessed->getType().illegalAccess().value()->getExpr()
				tp::TypeID param_type_id
					= ctx.lookupTypeInScopeAndParents(ctx.currNs().id, base::StrID("i64"));
				sym::SymbolID arg_sym_id
					= ctx.declareSymbol(param_accessed->getName(), sym::VariableSymbol{});
				param_type_ids.push_back(param_type_id);
				argument_symbol_ids.push_back(arg_sym_id);
			}
			ctx.popNamespace();

			tp::TypeID return_type_id
				= ctx.lookupTypeInScopeAndParents(ctx.currNs().id, base::StrID("void"));
			auto fun_type_id = ctx.newType(
				base::StrID(base::strConcat("$tp_", stmt->getName().str())),
				tp::Function{ .param_types = std::move(param_type_ids),
			                  .return_type = return_type_id }
			);
			ctx.insertSymbol(
				stmt->getName(),
				sym::FunctionSymbol{ .primary_namespace = func_ns_id,
			                         .argument_symbols  = std::move(argument_symbol_ids) },
				fun_type_id
			);
		}
	};
}
