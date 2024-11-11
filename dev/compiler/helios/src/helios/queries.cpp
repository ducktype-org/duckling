#include "queries.hpp"

#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>

#include <base/stable_hashmap.hpp>
#include <pst_parser/pst_visitor.hpp>

#include "base/exceptions.hpp"
#include "scopes/scopes.hpp"
#include "hout/elements.hpp"

#include "symbols/symbols.hpp"

namespace compiler::helios {

	void debugPrintScopeAndParents(ScopeID scope) {
		std::cerr << scope.customPerfectHash() << " -> ";
		while (parent(scope)) {
			scope = parent(scope).value();
			std::cerr << scope.customPerfectHash() << " -> ";
		}
		std::cerr << "\n";
	}

	struct IMPLEMENT_QUERY(QueryTopLevelEntities, HOUTUnit) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// go over all to level symbols and get theirs hout
			// store it in some vector or something
			// lookup all and stuff

			auto main_file_root_scope = extendQueryRootScopeOfMainModuleFile(ctx, key);

			auto symbols_in_module_root = ctx.query<QuerySymbolsInScope>(main_file_root_scope);

			HOUTUnit out;

			// grab constants:
			for (auto sym: symbols_in_module_root)
				if (kind(sym) == SymbolKind::Const) out.glob_data.emplace_back(sym, ctx);

			// grab functions:
			for (auto sym: symbols_in_module_root)
				if (kind(sym) == SymbolKind::Function)
					out.functions.push_back(ctx.query<QueryCodeOFFun>(sym));

			return out;
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTopLevelEntities);

	struct IMPLEMENT_QUERY(QueryCodeOFFun, HOUTFunction) {
		/**
		 * @brief Query extension to get hout CodeBlock from pst::CodeBlock or pst::CodeBlockOrStmt
		 * Might be changed into query in the future
		 */
		template<class Container>
		static auto queryCodeOfCodeBlock(query::Context& ctx, const Container& container) {
			auto            scope = ctx.query<QueryPrimaryCodeScopeFor>({ container });
			code::CodeBlock block(scope, {});
			for (const auto& stmt: container) {
				HoutStmtMaker stmt_maker(ctx);
				stmt->acceptVisitor(stmt_maker);
				if (not stmt_maker.empty)
					block.statements.emplace_back(std::move(stmt_maker.out.value()));
			}
			return block;
		}

		struct HoutStmtMaker final: public pst::PstStmtVisitorPanicky {
			query::Context&                              ctx;
			bool                                         empty = false;
			base::Optional<code::ElementRef<code::Stmt>> out;

			HoutStmtMaker(query::Context& ctx): ctx(ctx) {}

			template<class T>
			ScopeID scopeOf(const T& element) {
				return ctx.query<QueryPrimaryCodeScopeFor>({ PstRef<pst::LangElement>(&element) });
			}

			// @TODO: visits for all valid stmt-s

			// @TODO: some stuff in here are also symbols
			// "query symbol in scope" should be able to just work
			// and provide correct symbols for lookup, but same care
			// has to be taken, to ensure consistency between this code and scope states.

			template<class T>
			void output(T&& value) {
				this->out.emplace(
					base::make_unique<std::remove_reference_t<T>>(std::forward<T>(value))
				);
			}

			void visitReturn(const pst::Return& stmt) override {
				if (auto val = stmt.getValue()) {
					auto expr = ctx.query<QueryHoutOfExpr>({ val.value() });
					output(code::ReturnStmt(scopeOf(stmt), std::move(expr)));
				} else {
					output(code::VoidReturnStmt(scopeOf(stmt)));
				}
			}

			void visitAlias(const pst::Alias&) override { empty = true; }

			void visitUsing(const pst::Using&) override { empty = true; }

			void visitExprStmt(const pst::ExprStmt& stmt) override {
				// @EXPR
				CORE_PANIC("Not implemented yet...");
				// auto expr = ctx.query<QueryHoutOfExpr>({ PstRef<pst::Expr>(stmt.getExpr()) });
				// output(code::ExprStmt(scopeOf(stmt), std::move(expr)));
			}

			void visitIf(const pst::If& stmt) override {
				// Get scopes:
				auto outer_scope = scopeOf(stmt);

				// in the future we must also handle here different if-s variants
				// for example: `if (let a = ...) {}`.
				auto condition = ctx.query<QueryHoutOfExpr>({ stmt.getCondition() });

				auto body = queryCodeOfCodeBlock(ctx, stmt.getBody());

				output(code::IfStmt(outer_scope, std::move(condition), std::move(body)));
			}

			void visitVariable(const pst::Variable& stmt) override {
				// @TODO: do something with mut/immut

				// @note: This is a hot-path, that should work *most*
				// of the times. It will be changed during scope refactor.
				auto    stmt_parent        = stmt.getParent().value();
				auto    stmt_parent_parent = stmt_parent->getParent().value();
				ScopeID scope_of_symbol    = scopeOf(*stmt_parent);
				// @todo: change the usage of elementType to elementKind (once its implemented)
				// scope refactor will fix it
				if (stmt_parent_parent->elementType() == "Code Block or Statement"
				    and stmt_parent->elementType() == "Code Block") {
					scope_of_symbol = parent(scope_of_symbol).value();
				}

				// @TODO: error handling

				auto symbol
					= ctx.query<QuerySymbolOfSTMT>({ scope_of_symbol, PstRef<pst::Stmt>(&stmt) });

				auto symbol_type = ctx.query<QueryTypeOfSymbol>(symbol).expect(
					"Handling errors is not supported in HOUT yet"
				);

				// for now initial value is assumed to always be present:
				// this will probably change:
				auto initial_value = ctx.query<QueryHoutOfExpr>({ stmt.getValue() });

				output(code::VariableStmt(
					scope_of_symbol, std::move(initial_value), symbol_type, symbol
				));
			}
		};

		struct HOUTFunctionMaker final: public pst::PstStmtVisitorPanicky {
			query::Context& ctx;
			SymID           original_symbol;

			base::Optional<HOUTFunction> out;

			HOUTFunctionMaker(query::Context& ctx, SymID symbol):
				  ctx(ctx),
				  original_symbol(std::move(symbol)) {}

			void visitFun(const pst::Fun& stmt) final {
				// @TODO: create function here...
				// - create types, attributes, flags, ...
				// @TODO: params, rest, flags, attributes, etc

				HOUTFunction output(original_symbol, ctx);

				// Scope of function itself:
				// this scope will contain all "function declaration" symbols like parameters
				// auto outer_scope
				// 	= ctx.query<QueryPrimaryCodeScopeFor>({ PstRef<pst::LangElement>(&stmt) });

				auto fun_body = stmt.getBody();

				// HOUTCode out;
				code::CodeBlock function_body = queryCodeOfCodeBlock(ctx, fun_body);
				output.body.body
					= std::make_shared<const code::CodeBlock>(std::move(function_body));

				this->out.emplace(output);
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				kind(key) == SymbolKind::Function, "Function creation called on non-function symbol"
			);

			HOUTFunctionMaker func_maker(ctx, key);
			stmt(key)->acceptVisitor(func_maker);

			return func_maker.out.value();
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCodeOFFun);
}
