#include "queries.hpp"

#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>

#include <base/stable_hashmap.hpp>
#include <pst_parser/pst_visitor.hpp>

#include "scopes/scopes.hpp"
#include "hout/elements.hpp"

#include "symbols/symbols.hpp"

namespace compiler::helios {


	struct IMPLEMENT_QUERY(QueryTopLevelEntities, HOUTUnit) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// go over all to level symbols and get theirs hout
			// store it in some vector or something
			// lookup all and stuff

			auto root_scope = ctx.query<QueryRootScopeOf>(key);

			auto symbols_in_module_root
				= ctx.query<QuerySymbolsInScope>(root_scope);

			HOUTUnit out;

			// grab constants:
			for (auto sym: symbols_in_module_root) {
				if (kind(sym) == SymbolKind::Const) {
					auto original_name = name(sym);
					auto value = ctx.query<QueryConstValueOf>(sym);

					out.glob_data.push_back(HOUTGlobalData{
						sym, original_name, value
					});
				}
			}

			// grab functions:
			for (auto sym: symbols_in_module_root) {
				if (kind(sym) == SymbolKind::Function) {
					out.functions.push_back(ctx.query<QueryCodeOFFun>(sym));
				}
			}

			return out;
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTopLevelEntities);


	struct IMPLEMENT_QUERY(QueryCodeOFFun, HOUTFunction) {

		struct HoutStmtMaker: public pst::PstStmtVisitorPanicky {
			query::Context& ctx;
			ScopeID parent_scope;

			bool empty = false;
			base::Optional<code::ElementRef<code::Stmt>> out;

			HoutStmtMaker(query::Context& ctx, ScopeID scope): ctx(ctx), parent_scope(std::move(scope)) {}

			// @TODO: visits for all valid stmt-s

			// @krzyś
			// Na teraz: deklaracja zmiennej, if-y, expression

			void visitReturn(const pst::Return& stmt) override {
				if (auto val = stmt.getValue()) {
					auto expr = ctx.query<QueryHoutOfExpr>({
						 parent_scope, val.value() 
					});
					this->out.emplace(base::make_unique<code::ReturnStmt>(std::move(expr)));
				}
				else {
					this->out.emplace(base::make_unique<code::VReturnStmt>());
				}
			}

			void visitAlias(const pst::Alias&) override { empty = true; }
			void visitUsing(const pst::Using&) override { empty = true; }
		};

		struct HOUTFunctionMaker: public pst::PstStmtVisitorPanicky {
			query::Context& ctx;
			ScopeID parent_scope;
			
			base::Optional<HOUTFunction> out;

			HOUTFunctionMaker(query::Context& ctx, ScopeID scope): ctx(ctx), parent_scope(scope) {}

			void visitFun(const pst::Fun& stmt) final {
				// @TODO: create function here...
				// - create types, attributes, flags, ...
				// @TODO: params, rest, flags, attributes, etc

				HOUTFunction output;
				output.original_name = stmt.getName();

				// Scope of function itself:
				// this scope will contain all "function declaration" symbols like parameters
				// @TODO: document somewhere how do scopes behave depending on what they are looking at
				auto outer_scope = ctx.query<QueryPrimaryCodeScopeFor>(
					{ parent_scope, PstRef<pst::RiftElement>(&stmt) }
				);

				auto fun_body = stmt.getBody();

				// Scope of function body:
				auto inner_scope = ctx.query<QueryPrimaryCodeScopeFor>(
					{ outer_scope, PstRef<pst::RiftElement>(fun_body) }
				);

				// HOUTCode out;
				code::CodeBlock function_body;
				for (const auto& code_stmt: fun_body) {
					HoutStmtMaker stmt_maker(ctx, inner_scope);
					code_stmt->acceptVistior(stmt_maker);
					if (not stmt_maker.empty) {
						function_body.statements.emplace_back(std::move(stmt_maker.out.value()));
					}
				}

				output.body.body = std::make_shared<const code::CodeBlock>(std::move(function_body));

				this->out.emplace(output);
			}
		};
		
		static auto provide(Context& ctx, QKey key) -> PResult {
			RIFT_ASSERT(kind(key) == SymbolKind::Function, "Function creation called on non-function symbol");

			// auto fun_stmt = dynamic_cast<const pst::Fun*>(stmt(key).get());
			// RIFT_ASSERT(fun_stmt != nullptr, "Function symbol is not actually a function");
			auto parent_scope = scope(key);

			HOUTFunctionMaker func_maker(ctx, parent_scope);
			stmt(key)->acceptVistior(func_maker);

			return func_maker.out.value();
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCodeOFFun);


}
