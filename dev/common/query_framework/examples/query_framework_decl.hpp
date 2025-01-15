#pragma once

#include <base/perfect_hash.hpp>
#include <query_framework/query_int.hpp>
#include <string>  // std::string

/**
 * Query Key.
 * note that it does not have to declared here.
 */
struct Key {
	/* ... */

	// for example:
	uint64_t v;

	[[nodiscard]]
	constexpr base::HashT customPerfectHash() const noexcept {
		return v;
	}

	[[nodiscard]]
	constexpr std::strong_ordering operator<=>(const Key& other) const noexcept {
		return v <=> other.v;
	}
};

/**
 * Query Value.
 * note that it does not have to declared here.
 */
struct Value {
	/*...*/

	// for example:
	uint64_t v;
};

/**
 * Query declaration.
 * Under the hood it will create a struct called `MyQuery`.
 */
DECLARE_QUERY(MyQuery, Key, Value)

/**
 * This query just takes uint64_t as an argument and returns std::string.
 */
DECLARE_QUERY(Query2, u64, std::string)
