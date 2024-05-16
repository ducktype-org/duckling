#include "queries.hpp"

#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>

#include <base/stable_hashmap.hpp>

#include "scopes/scopes.hpp"

namespace compiler::helios {


	struct ImplementationOf_QueryModuleHOUT:
		  public query::QueryImplementation<QueryModuleHOUT, HOUTModule> {
		static auto provide(Context&, QKey key) -> PResult {
			// go over all to level symbols and get theirs hout
			// store it in some vector or something
			// lookup all and stuff

			auto&& root_scope = query::entryPoint<QueryRootScopeOf>(key);

			auto&& module_file = query::entryPoint<frontend::QueryMainSourceFile>(key);
			// TODO: Here adding PSTs of other files would come into play.

			auto&& main_pst = query::entryPoint<frontend::QueryFilePST>(module_file);

			auto&& module_root_scope_id
				= query::entryPoint<QueryPrimaryCodeScopeFor>({ root_scope,
			                                                    main_pst.getTopLevelElement() });

			auto&& symbols_in_submodule
				= query::entryPoint<QuerySymbolsInScope>(module_root_scope_id);

			// @TODO: set imported modules here
			return { symbols_in_submodule, {} };
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};


}
