/**
 * @file queries.hpp
 * @brief The queries that decide coercions.
 *
 * There are two layers here:
 * - `QuerySymbolTypeCoercion`:
 * Builds the `SymbolTypeCoercion` from the two types alone. Which
 * includes the type conversions, reference kinds and mutability. The ownership is left undecided.
 * @see rules.cpp
 *
 * - `coercionOf`:
 * Adds the `ValueCategory` into consideration and handles the ownership related
 * stuff. This is the main compiler entry point.
 * @see `ownership.cpp`.
 *
 * Only the type layer is a query since a `ValueSource` holds a wide content of values and a
 * probability of a cache-hit on it is low.
 */

#pragma once

#include "coercion.hpp"
#include "value_source.hpp"

#include <hashing/hash.hpp>
#include <query_framework/query_int.hpp>

namespace compiler::tsh::coercions {
	/**
	 * @brief Key for QuerySymbolTypeCoercion.
	 */
	struct KeyFor_QuerySymbolTypeCoercion final {
		/**
		 * @brief The type of the value being coerced.
		 */
		SymbolType<> source;

		/**
		 * @brief The type of the location the value is handed to.
		 */
		SymbolType<> target;

		KeyFor_QuerySymbolTypeCoercion(const SymbolType<>& source, const SymbolType<>& target):
			  source(source),
			  target(target) {}

		[[nodiscard]]
		auto operator<=>(const KeyFor_QuerySymbolTypeCoercion&) const
			= default;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(source, target);
		}
	};

	/**
	 * @brief Everything the type system decides about coercing a value of the source type into a
	 * location of the target type from the types alone.
	 *
	 * This is a whole coercion except for the ownership decisions, which need the value rather than
	 * its type. The places they are made at are the `HandOver` steps.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		QuerySymbolTypeCoercion,
		KeyFor_QuerySymbolTypeCoercion,
		CRef<SymbolTypeCoercion>,
		({ .uses_qresult = false })
	)

	/**
	 * @brief Everything the type system decides about handing one value over to a
	 * location of the target type. Queries the coercion plan from `QuerySymbolTypeCoercion` and
	 * decides every `HandOver` of it for @p source.
	 *
	 * @TODO: #1920 Convert array literals into ValueSource when performing coercions.
	 *
	 * @return A full coercion that can be performed by HELIoS.
	 */
	[[nodiscard]] Coercion coercionOf(
		query::Context& ctx, const ValueSource& source, const SymbolType<>& target
	);
}
