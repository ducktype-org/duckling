#include "queries.hpp"

#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>

#include <base/stable_hashmap.hpp>

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

		
		static auto provide(Context& ctx, QKey key) -> PResult {
			RIFT_ASSERT(kind(key) == SymbolKind::Function, "Function creation called on non-function symbol");

			auto fun_stmt = dynamic_cast<const pst::Fun*>(stmt(key).get());
			RIFT_ASSERT(fun_stmt != nullptr, "Function symbol is not actually a function");

			auto parent_scope = scope(key);

			// @TODO


			HOUTFunction out;

			return out;
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCodeOFFun);


}
