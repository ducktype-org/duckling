#pragma once

#include <base/types/ints.hpp>

namespace query {
	/**
	 * @brief Key used for queries without keys, input queries, and "outside world" query.
	 */
	struct EmptyKey final {
		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return 0;
		}
	};
}
