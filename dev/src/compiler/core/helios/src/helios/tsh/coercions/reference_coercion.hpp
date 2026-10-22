// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file reference_coercion.hpp
 * @brief What a coercion may do to the reference part of a value.
 */

#pragma once

#include "../symbol_type.hpp"

#include <cstdint>
#include <string>

namespace compiler::tsh::coercions {
	/**
	 * @brief What has to happen to the reference part of a value to reach the target kind.
	 */
	enum class ReferenceAdjustment : uint8_t {
		/// Reference kinds match.
		None,
		/// `ref`/`box` -> `direct`. Deref.
		Deref,
		/// `direct` -> `ref`/`box`, `ref` -> `box`, `box`-> `ref`. Needs `&` or `new`.
		Illegal,
	};

	/**
	 * @brief Everything a coercion needs to know about a reference-kind coercion.
	 */
	struct ReferenceCoercion final {
		[[nodiscard]]
		static ReferenceCoercion illegal() {
			return ReferenceCoercion{
				.adjustment        = ReferenceAdjustment::Illegal,
				.creates_new_value = false,
				.points_to_source  = false,
			};
		}

		ReferenceAdjustment adjustment;

		/**
		 * @brief Whether this coercion a new value, which then has to be copied or moved out.
		 */
		bool creates_new_value;

		/**
		 * @brief Whether the resulting value points at the source's storage, so that writing
		 * through the result writes to the original.
		 *
		 * @TODO: #1488 Make use of it.
		 */
		bool points_to_source;

		[[nodiscard]]
		bool isLegal() const noexcept {
			return adjustment != ReferenceAdjustment::Illegal;
		}

		/**
		 * @brief A human readable description of the reference kind transition.
		 */
		[[nodiscard]] std::string toString() const;

		[[nodiscard]]
		auto operator<=>(const ReferenceCoercion&) const
			= default;
	};

	/**
	 * @brief The rule for coercing a value of ref kind @p from into kind @p to.
	 *
	 * The current rules are:
	 * ```
	 *                        FROM
	 *              | Direct | Ref | Box
	 *      Direct  |  Yes   | Yes | Yes
	 *  TO  Ref     |   No   | Yes | No
	 *      Box     |   No   | No  | Yes
	 * ```
	 */
	[[nodiscard]] ReferenceCoercion referenceCoercionRule(ReferenceKind from, ReferenceKind to);
}
