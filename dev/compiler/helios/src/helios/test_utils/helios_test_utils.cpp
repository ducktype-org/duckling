#include "helios_test_utils.hpp"

#include <helios_private/query_hout_of_expr.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <helios/symbols/query_type_from_definition.hpp>

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <pst_parser/pst_visitor.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <frontend/module_tree/queries.hpp>


#include <base/anycast.hpp>

namespace compiler::helios::test_utils {
	std::pair<frontend::ModuleID, ScopeID> getModule(const fs::FilePath& path) {
		auto module = query::entryPoint<frontend::QueryModuleTree>(path);

		auto main_file_root_scope = query::utils::withContextCompute([&](query::Context& ctx) {
			return queryRootScopeOfMainModuleFile(ctx, module);
		});

		return { module, base::anyCast<ScopeID>(main_file_root_scope) };
	}

	std::vector<SymID> getChain(const std::string_view chain, ScopeID scope) {
		auto       symbols = base::strSplit(chain, ".");
		SymbolList result;
		bool       first_symbol = true;
		for (auto&& sym: symbols) {
			auto symbol      = first_symbol ? query::entryPoint<QueryLookupInScopeAndParents>(
                                             { scope, base::StrID(sym.c_str()), true }
                                         )
			                                : query::entryPoint<QueryLookupInSymbol>(
                                             { result.back(), base::StrID(sym.c_str()), false }

                                         );
			auto symbol_path = symbol->getAsSingle().valueOrThrow();
			for (auto&& elem: symbol_path) {
				auto dealiased = query::entryPoint<QueryDealias>(elem)->valueOrThrow();
				result.insert(result.end(), dealiased.begin(), dealiased.end());
			}
			first_symbol = false;
		}
		return result;
	}

	i64 getValue(const std::string_view chain, ScopeID scope) {
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
			errors::HResult<base::Box<code::Expr>, errors::Failed> expr_tree;

			void setExprTree(pst::AccessLocked<pst::ExprElement> expr) {
				expr_tree = query::entryPoint<QueryHoutOfExpr>(expr);
			}

		public:
			void visitConst(pst::Access<pst::Const> stmt) override {
				setExprTree(stmt->getValue().illegalAccess().value()->getExpr());
			}
		};

		auto            pst_stmt = symbolPst(sym).illegalAccess().value();
		GetHOUTExprTree visitor;
		pst_stmt->acceptVisitor(visitor);

		return std::move(visitor.expr_tree).value();
	}

	Box<code::Expr> getExprOfVariable(SymID sym) {
		struct GetHOUTExprTree final: public pst::PstVisitorPanicky {
			errors::HResult<base::Box<code::Expr>, errors::Failed> expr_tree;

			void setExprTree(pst::AccessLocked<pst::ExprElement> expr) {
				expr_tree = query::entryPoint<QueryHoutOfExpr>(expr);
			}

		public:
			void visitVariable(pst::Access<pst::Variable> stmt) override {
				setExprTree(stmt->getValue().illegalAccess().value()->getExpr());
			}
		};

		auto            pst_stmt = symbolPst(sym).illegalAccess().value();
		GetHOUTExprTree visitor;
		pst_stmt->acceptVisitor(visitor);

		return std::move(visitor.expr_tree).value();
	}
}
