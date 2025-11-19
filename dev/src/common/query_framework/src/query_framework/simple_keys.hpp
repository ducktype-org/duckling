/**
 * Some simple key types for queries.
 */

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

		[[nodiscard]]
		u64 queryStablePerfectHash() const {
			return 0;
		}
	};

	/**
	 * @brief Simple key wrapping a u64 value.
	 */
	struct U64Key final {
		u64 value;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return value;
		}

		[[nodiscard]]
		u64 queryStablePerfectHash() const {
			return value;
		}
	};

	/**
	 * @brief Simple key wrapping a bool value.
	 */
	struct BoolKey final {
		bool value;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return static_cast<u64>(value);
		}

		[[nodiscard]]
		u64 queryStablePerfectHash() const {
			return static_cast<u64>(value);
		}
	};


}
