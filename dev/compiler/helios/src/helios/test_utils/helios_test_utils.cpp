#include "helios_test_utils.hpp"

#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <pst_parser/pst_visitor.hpp>
#include <helios/hout/elements/query_hout_of_expr.hpp>

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
		return query::entryPoint<QueryConstValueOf>(getChain(chain, scope).back())->valueOrThrow();
	}

	tsh::TypeInfo getTypeOf(const std::string_view chain, ScopeID scope) {
		return query::entryPoint<QueryTypeOfSymbol>(getChain(chain, scope).back())->valueOrThrow();
	}

	tsh::TypeInfo getTypeFromDefinition(const std::string_view chain, ScopeID scope) {
		return query::entryPoint<QueryTypeFromDefinition>(getChain(chain, scope).back())
		    ->valueOrThrow();
	}

	Box<code::Expr> getExprOfConst(SymID sym) {
		struct GetHOUTExprTree final: public pst::PstVisitorPanicky {
			errors::HResult<base::Box<code::Expr>, errors::Failed> expr_tree;

			void setExprTree(const MCRef<pst::ExprElement>& expr) {
				expr_tree = query::entryPoint<QueryHoutOfExpr>({ expr });
			}

		public:
			void visitConst(const pst::Const& stmt) override {
				setExprTree(stmt.getValue()->getExpr());
			}
		};

		auto            pst_stmt = stmt(sym);
		GetHOUTExprTree visitor;
		pst_stmt->acceptVisitor(visitor);

		return std::move(visitor.expr_tree).value();
	}

	Box<code::Expr> getExprOfVariable(SymID sym) {
		struct GetHOUTExprTree final: public pst::PstVisitorPanicky {
			errors::HResult<base::Box<code::Expr>, errors::Failed> expr_tree;

			void setExprTree(const MCRef<pst::ExprElement>& expr) {
				expr_tree = query::entryPoint<QueryHoutOfExpr>({ expr });
			}

		public:
			void visitVariable(const pst::Variable& stmt) override {
				setExprTree(stmt.getValue()->getExpr());
			}
		};

		auto            pst_stmt = stmt(sym);
		GetHOUTExprTree visitor;
		pst_stmt->acceptVisitor(visitor);

		return std::move(visitor.expr_tree).value();
	}
}
