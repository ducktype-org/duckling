#pragma once

#include <base/types/ints.hpp>

#include <query_framework/query_int.hpp>

#include <string>  // std::string

/**
 * Query Key.
 * note that it does not have to declared here.
 */
struct Key final {
	/* any key here */

	// for example:
	u64 v;

	[[nodiscard]] u64 queryUnstablePerfectHash() const { return v; }
};

/**
 * Query Value.
 * note that it does not have to declared here.
 */
struct Value {
	/* any value here */

	// for example:
	u64 v;
};

/**
 * Query declaration.
 * Under the hood it will create a struct called `MyQuery` which is the query interface struct.
 */
DECLARE_QUERY(MyQuery, Key, Value, ({ .uses_qresult = false }))

/**
 * Another query. This query returns std::string as a result.
 */
DECLARE_QUERY(Query2, Key, std::string, ({ .uses_qresult = false }))
