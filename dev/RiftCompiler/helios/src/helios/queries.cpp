#include "queries.hpp"

#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>

#include <base/stable_hashmap.hpp>
#include <pst_parser/pst_visitor.hpp>

#include "scopes/scopes.hpp"

#include "symbols/symbols.hpp"

namespace compiler::helios {


	struct IMPLEMENT_QUERY(QueryTopLevelFunctions, HOUTUnit) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// go over all to level symbols and get theirs hout
			// store it in some vector or something
			// lookup all and stuff

			auto&& root_scope = ctx.query<QueryRootScopeOf>(key);
			auto&& module_file = ctx.query<frontend::QueryMainSourceFile>(key);

			auto&& main_pst = ctx.query<frontend::QueryFilePST>(module_file);

			auto&& module_root_scope_id = ctx.query<QueryPrimaryCodeScopeFor>(
				{ root_scope, main_pst.getTopLevelElement() }
			);

			auto&& symbols_in_submodule
				= ctx.query<QuerySymbolsInScope>(module_root_scope_id);


			HOUTUnit out;

			// grab functions:
			for (auto sym: symbols_in_submodule) {
				if (kind(sym) == SymbolKind::Function) {
					out.functions.push_back(ctx.query<QueryCodeOFFun>(sym));
				}
			}

			return out;
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTopLevelFunctions);


	struct IMPLEMENT_QUERY(QueryCodeOFFun, HOUTFunction) {

		struct HoutStmtMaker: public pst::PstStmtVisitorPanicky {
			query::Context& ctx;
			ScopeID parent_scope;

			base::Optional<code::ElementRef<code::Stmt>> out;

			HoutStmtMaker(query::Context& ctx, ScopeID scope): ctx(ctx), parent_scope(scope) {}

			// @TODO: visits for all valid stmt-s

		};

		struct HOUTFunctionMaker: public pst::PstStmtVisitorPanicky {
			query::Context& ctx;
			ScopeID parent_scope;
			
			base::Optional<HOUTFunction> out;

			HOUTFunctionMaker(query::Context& ctx, ScopeID scope): ctx(ctx), parent_scope(scope) {}

			void visitFun(const pst::Fun& stmt) final {
				// @TODO: create function here...
				// - create types, attributes, flags, ...
				// - crete code via additional visitor

				// @TODO: crete inner scope and run FunctionCodeMaker on function body

				HOUTFunction output;
				output.original_name = stmt.getName();

				// @TODO: params, rest, flags, attributes, etc

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
					function_body.statements.emplace_back(std::move(stmt_maker.out.value()));
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
