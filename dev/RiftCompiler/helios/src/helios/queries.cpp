#include "queries.hpp"
#include <query_framework/query_impl.hpp>
#include <base/stable_hashmap.hpp>

namespace compiler::helios {


	struct ImplementationOf_QueryModuleHOUT: public query::QueryImplementation<QueryModuleHOUT, HOUTModule> {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// go over all to level symbols and get theirs hout
			// store it in some vector or something
			// lookup all and stuff

			throw "@TODO";
		}

		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};


}

