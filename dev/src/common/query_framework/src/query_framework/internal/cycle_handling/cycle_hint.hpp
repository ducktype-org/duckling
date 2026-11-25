// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace query::internal {

	/**
	 * @brief A guess at what in the program caused a query cycle, for the cycle diagnostic.
	 *
	 * This is only a heuristic: it looks at the names of the queries on the cycle. It returns an
	 * empty string when it has no guess.
	 * @TODO: #2615 Replace it with descriptions provided by the queries themselves.
	 */
	inline std::string cycleHint(const std::vector<std::string_view>& query_names) {
		const auto has
			= [&](std::string_view name) { return std::ranges::contains(query_names, name); };

		std::string hint;
		if (has("QueryReturnTypeDeduction"))
			hint += "Hint: maybe a function without a declared return type calls itself, directly "
			        "or through other functions. Declare its return type, e.g. "
			        "`fun f(n: i64) -> i64 = ...`.\n";
		if (has("QueryTypeOfSymbol"))
			hint += "Hint: maybe a variable without a declared type is used in its own "
			        "initializer. This also happens when a `;` is missing and the next line is "
			        "read as a part of the initializer, e.g. `var x = 1` followed by `print(x);`.\n";
		return hint;
	}
}
