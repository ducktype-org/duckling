#include "queries.hpp"
#include "query_framework/query_entry_point.hpp"
#include "scopes/scopes.hpp"

#include <query_framework/query_impl.hpp>
#include <base/stable_hashmap.hpp>

namespace compiler::helios {


	struct ImplementationOf_QueryModuleHOUT:
		  public query::QueryImplementation<QueryModuleHOUT, HOUTModule> {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// go over all to level symbols and get theirs hout
			// store it in some vector or something
			// lookup all and stuff

			auto&& root_scope = query::queryEntryPoint<QueryRootScopeOf>(key);

			auto&& module_file = query::queryEntryPoint<frontend::QueryMainSourceFile>(key);
			// TODO: Here adding PSTs of other files would come into play.

			auto&& main_pst = query::queryEntryPoint<frontend::QueryFilePST>(module_file);

			auto&& module_root_scope_id = query::queryEntryPoint<QueryPrimaryCodeScopeFor>(
				{ root_scope, main_pst.getTopLevelElement() }
			);

			auto&& symbols_in_submodule
				= query::queryEntryPoint<QuerySymbolsInScope>(module_root_scope_id);

			return { symbols_in_submodule };
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};


}
