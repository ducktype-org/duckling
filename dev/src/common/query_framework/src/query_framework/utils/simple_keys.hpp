// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * Some simple key types for queries.
 */

#pragma once

#include <base/types/bit256.hpp>
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

	/**
	 * @brief Simple key wrapping a u64 value.
	 */
	struct U64Key final {
		u64 value;

		U64Key() = delete;

		U64Key(u64 value): value(value) {}

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return value;
		}

		[[nodiscard]]
		base::Bit256 queryStablePerfectHash() const {
			return value;
		}
	};

	/**
	 * @brief Simple key wrapping a bool value.
	 */
	struct BoolKey final {
		bool value;

		BoolKey() = delete;

		BoolKey(bool value): value(value) {}

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return static_cast<u64>(value);
		}

		[[nodiscard]]
		base::Bit256 queryStablePerfectHash() const {
			return static_cast<u64>(value);
		}
	};
}
