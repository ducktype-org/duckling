#include "helios_test_utils.hpp"

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/round_group_expression.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/lookup/interface.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/anycast.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>

namespace compiler::helios::test_utils {
	std::pair<frontend::ModuleID, ScopeID> getModule(const fs::File& path) {
		auto module = compiler::frontend::createModuleTreeWithRandomPackageID(path);

		auto main_file_root_scope = query::utils::withContextCompute([&](query::Context& ctx) {
			return queryRootScopeOfMainModuleFile(ctx, module);
		});

		return { module, base::anyCast<ScopeID>(main_file_root_scope) };
	}

	ScopeID getModuleScope(frontend::ModuleID module) {
		auto main_file_root_scope = query::utils::withContextCompute([&](query::Context& ctx) {
			return queryRootScopeOfMainModuleFile(ctx, module);
		});

		return base::anyCast<ScopeID>(main_file_root_scope);
	}

	SymbolList getChain(const std::string_view chain, ScopeID scope) {
		auto symbols = base::strSplit(chain, ".");

		auto computed = query::utils::withContextCompute([&](query::Context& ctx) {
			SymbolList result;
			bool       first_symbol = true;
			for (auto&& sym: symbols) {
				auto name = base::StrID(sym.c_str());

				auto lookup_qresult = first_symbol
				                        ? HInterface::ofScopeWithParents(scope).lookup(
											  ctx, name, { .with_wildcards = true }
										  )
				                        : HInterface::ofSymbol(ctx, result.back())
				                              .lookup(ctx, name, { .with_wildcards = false });

				CRef<LookupResult> symbol = &lookup_qresult->valueOrThrow();
				CORE_ASSERT(symbol->isSingle(), "Expected single symbol in chain lookup");

				auto symbol_path_variant = symbol->getAsSingle().valueOrPanic();
				CORE_ASSERT(
					v_matches(symbol_path_variant, SymbolList),
					"Expected single symbol in chain lookup"
				);

				result.appendList(std::get<SymbolList>(symbol_path_variant));
				first_symbol = false;
			}
			return result;
		});

		return base::anyCast<SymbolList>(computed);
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
		return tsh::SymbolType<>::withDefaults(
			query::entryPoint<tsh::QueryClassType>(getChain(chain, scope).back())
		);
	}

	Box<code::Expr> getExprOfConst(SymID sym) {
		struct GetHOUTExprTree final: public pst::PstVisitorPanicky {
			base::Optional<query::QResult<Box<code::Expr>>> expr_tree;

			void setExprTree(pst::AccessLocked<pst::ExprElement> expr) {
				CORE_ASSERT(!expr_tree.has_value(), "Expr tree already set");
				expr_tree.emplace(query::entryPoint<QueryHoutOfExpr>(expr)->valueOrPanic()->clone());
			}

		public:
			void visitConst(pst::Access<pst::Const> stmt) override {
				CORE_ASSERT(stmt->getValue().has_value(), "Visited Const had no declared value");
				setExprTree(stmt->getValue().value().illegalAccess().value()->getExpr());
			}
		};

		auto            pst_stmt = maybeSymbolPst(sym).value().illegalAccess().value();
		GetHOUTExprTree visitor;
		pst_stmt->acceptVisitor(visitor);

		return std::move(visitor.expr_tree.value()).valueOrThrow();
	}

	Box<code::Expr> getExprOfVariable(SymID sym) {
		struct GetHOUTExprTree final: public pst::PstVisitorPanicky {
			base::Optional<query::QResult<Box<code::Expr>>> expr_tree;

			void setExprTree(pst::AccessLocked<pst::ExprElement> expr) {
				CORE_ASSERT(!expr_tree.has_value(), "Expr tree already set");
				expr_tree.emplace(query::entryPoint<QueryHoutOfExpr>(expr)->valueOrPanic()->clone());
			}

		public:
			void visitVariable(pst::Access<pst::Variable> stmt) override {
				CORE_ASSERT(stmt->getValue().has_value(), "Visited Variable had no declared value");
				setExprTree(stmt->getValue().value().illegalAccess().value()->getExpr());
			}
		};

		auto            pst_stmt = maybeSymbolPst(sym).value().illegalAccess().value();
		GetHOUTExprTree visitor;
		pst_stmt->acceptVisitor(visitor);

		return std::move(visitor.expr_tree.value()).valueOrThrow();
	}

	ScopeID getFunctionBodyScope(SymID sym) {
		return base::anyCast<ScopeID>(
			query::utils::withContextCompute([&](query::Context& ctx) -> std::any {
				auto func_pst
					= maybeSymbolPst(sym).value().unlock(ctx).dynamicCast<pst::Fun>().value();
				auto fun_body = func_pst->getBody().unlock(ctx);
				return ctx.query<QueryPrimaryCodeScopeFor>(fun_body);
			})
		);
	}
}
