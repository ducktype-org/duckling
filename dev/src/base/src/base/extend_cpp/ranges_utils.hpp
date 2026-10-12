// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include <ranges>

namespace base {
	/**
	 * @brief Returns a lazy view with @p delim inserted between every element.
	 *
	 * For example [a,b,c,d] and delim = "t" we get [a,t,b,t,c,t,d].
	 * Usege: `v | intersperse(std::string("x"))`
	 *
	 * @tparam Delimiter type.
	 * @param delim Value inserted between adjacent elements.
	 * @return A range adaptor closure.
	 */
	auto rangesIntersperse(auto delim) {
		// Given (a,b,c,d), t
		// transform ((a,t), (b,t), (c,t), (d,t))
		// join (a,t,b,t,c,t,d,t)
		// drop last (a,t,b,c,t,d)
		return std::views::transform([delim = std::move(delim)](auto&& val) {
				   return std::array<decltype(delim), 2>{ delim, std::forward<decltype(val)>(val) };
			   })
		     | std::views::join | std::views::drop(1);
	}

}
