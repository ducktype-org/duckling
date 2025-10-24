#include "helios_test_utils.hpp"

#include <frontend/module_tree/module_tree.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <pst_parser/elements/hierarchy/not_statements/class_block.hpp>
#include <pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <pst_parser/elements/hierarchy/not_statements/round_group_expression.hpp>
#include <pst_parser/pst_visitor.hpp>

#include <base/misc/anycast.hpp>

#include <query_framework/context.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

namespace compiler::helios::test_utils {
	std::pair<frontend::ModuleID, ScopeID> getModule(const fs::File& path) {
		auto module = compiler::frontend::createModuleTreeWithRandomPackageID(path);

		auto main_file_root_scope = query::utils::withContextCompute([&](query::Context& ctx) {
			return queryRootScopeOfMainModuleFile(ctx, module);
		});

		return { module, base::anyCast<ScopeID>(main_file_root_scope) };
	}

	SymbolList getChain(const std::string_view chain, ScopeID scope) {
		auto       symbols = base::strSplit(chain, ".");
		SymbolList result;
		bool       first_symbol = true;
		for (auto&& sym: symbols) {
			auto symbol = first_symbol ? query::entryPoint<QueryLookupInScopeAndParents>(
											 { scope, base::StrID(sym.c_str()), true }
										 )
			                           : query::entryPoint<QueryLookupInSymbol>(
											 { result.back(), base::StrID(sym.c_str()), false }

										 );
			CORE_ASSERT(symbol->isSingle(), "Expected single symbol in chain lookup");
			auto symbol_path = symbol->getAsSingle().valueOrThrow();
			for (auto&& elem: symbol_path) {
				auto dealiased = query::entryPoint<QueryDealias>(elem)->valueOrThrow();
				result.appendList(dealiased);
			}
			first_symbol = false;
		}
		return result;
	}

	ctv::CompileTimeValue getConstValue(const std::string_view chain, ScopeID scope) {
		return query::entryPoint<QueryConstValueOf>(getChain(chain, scope).back()).valueOrThrow();
	}

	tsh::SymbolType<> getSymbolTypeOf(const std::string_view chain, ScopeID scope) {
		return query::entryPoint<QueryTypeOfSymbol>(getChain(chain, scope).back())->valueOrThrow();
	}

	tsh::AbstractType getTypeOf(const std::string_view chain, ScopeID scope) {
		return getSymbolTypeOf(chain, scope).getType();
	}

	tsh::SymbolType<> getTypeFromDefinition(const std::string_view chain, ScopeID scope) {
		return query::entryPoint<QueryTypeFromDefinition>(getChain(chain, scope).back())
		    ->valueOrThrow();
	}

	Box<code::Expr> getExprOfConst(SymID sym) {
		struct GetHOUTExprTree final: public pst::PstVisitorPanicky {
			base::Optional<query::QResult<Box<code::Expr>, errors::Failed>> expr_tree;

			void setExprTree(pst::AccessLocked<pst::ExprElement> expr) {
				CORE_ASSERT(!expr_tree.has_value(), "Expr tree already set");
				expr_tree.emplace(query::entryPoint<QueryHoutOfExpr>(expr));
			}

		public:
			void visitConst(pst::Access<pst::Const> stmt) override {
				CORE_ASSERT(stmt->getValue().has_value(), "Visited Const had no declared value");
				setExprTree(stmt->getValue().value().illegalAccess().value()->getExpr());
			}
		};

		auto            pst_stmt = symbolPst(sym).illegalAccess().value();
		GetHOUTExprTree visitor;
		pst_stmt->acceptVisitor(visitor);

		return std::move(visitor.expr_tree.value()).value();
	}

	Box<code::Expr> getExprOfVariable(SymID sym) {
		struct GetHOUTExprTree final: public pst::PstVisitorPanicky {
			base::Optional<query::QResult<Box<code::Expr>, errors::Failed>> expr_tree;

			void setExprTree(pst::AccessLocked<pst::ExprElement> expr) {
				CORE_ASSERT(!expr_tree.has_value(), "Expr tree already set");
				expr_tree.emplace(query::entryPoint<QueryHoutOfExpr>(expr));
			}

		public:
			void visitVariable(pst::Access<pst::Variable> stmt) override {
				CORE_ASSERT(stmt->getValue().has_value(), "Visited Variable had no declared value");
				setExprTree(stmt->getValue().value().illegalAccess().value()->getExpr());
			}
		};

		auto            pst_stmt = symbolPst(sym).illegalAccess().value();
		GetHOUTExprTree visitor;
		pst_stmt->acceptVisitor(visitor);

		return std::move(visitor.expr_tree.value()).value();
	}

	ScopeID getFunctionBodyScope(SymID sym) {
		return base::anyCast<ScopeID>(
			query::utils::withContextCompute([&](query::Context& ctx) -> std::any {
				auto func_pst = symbolPst(sym).unlock(ctx).dynamicCast<pst::Fun>().value();
				auto fun_body = func_pst->getBody().unlock(ctx);
				return ctx.query<QueryPrimaryCodeScopeFor>(fun_body);
			})
		);
	}
}
