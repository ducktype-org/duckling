#pragma once

#include "query_hash.hpp"

namespace query {
	/**
	 * @brief Key used for queries without keys, input queries, and "outside world" query.
	 */
	struct EmptyKey final {
		[[nodiscard]]
		QueryUnstableHash queryUnstablePerfectHash() const {
			return 0;
		}
	};
}
