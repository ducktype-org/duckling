#pragma once

#include <base/perfect_hash.hpp>

namespace query {
	/**
	 * @brief Key used for queries without keys, input queries, and "outside world" query.
	 */
	struct EmptyKey {
		base::HashT customPerfectHash() { return 0; }
	};
}
