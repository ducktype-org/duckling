#include "queries.hpp"
#include <hashing/combine.hpp>

namespace compiler::frontend::packages {

    KeyOf_QueryPackageSideInput::queryStablePerfectHash() const {
        hashing::ComponentHash::HashAlg hasher;
		hashing::addToHash(hasher, package_id);
		return hasher.finalize();
    }

    IMPLEMENT_QUERY_SIDE_INPUT(QueryPackageSideInput);

} // namespace compiler::frontend::packages