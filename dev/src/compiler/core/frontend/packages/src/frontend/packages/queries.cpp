#include "queries.hpp"

#include <hashing/add_to_hash.hpp>
#include <hashing/component_hash.hpp>
#include <hashing/hash.hpp>
#include <query_framework/input_query/query_input_impl.hpp>

namespace compiler::frontend::packages {

	query::QueryStableHash KeyOf_QueryPackageSideInput::queryStablePerfectHash() const {
		hashing::ComponentHash::HashAlg hasher;
		hashing::addToHash(hasher, package_id);
		return hasher.finalize();
	}

	IMPLEMENT_QUERY_SIDE_INPUT(QueryPackageSideInput);

}  // namespace compiler::frontend::packages
