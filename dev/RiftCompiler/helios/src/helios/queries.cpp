#include "queries.hpp"
#include <query_framework/query_impl.hpp>
#include <base/stable_hashmap.hpp>

namespace compiler::helios {


	struct ImplementationOf_QueryModuleHOUT: public query::QueryImplementation<QueryModuleHOUT, HOUTModule> {



		QUERY_AUTO_CACHE_PRESULT_STABLE_REF
	};


}

