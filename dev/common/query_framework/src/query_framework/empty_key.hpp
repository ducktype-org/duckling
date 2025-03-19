#pragma once

#include <base/perfect_hash.hpp>

namespace query {
	/**
	 * @brief Key used for queries without keys, input queries, and "outside world" query.
	 */
	struct EmptyKey final {
		[[nodiscard]]
		base::HashT customPerfectHash() const {
			return 0;
		}

		[[nodiscard]]
		bool operator==(const EmptyKey&) const {
			return true;
		}
	};
}
