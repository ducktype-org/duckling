// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/pst_parser/access.hpp>

#include <query_framework/query_errors.hpp>

namespace compiler::helios {
	/**
	 * @brief Unlocks @p element, or fails the query without a diagnostic if it is null. A null
	 * element comes from a syntax error, which the PST has already reported.
	 */
	template<class Element>
	pst::Access<Element> unlockOrFail(query::Context& ctx, pst::AccessLocked<Element> element) {
		auto unlocked = element.unlockOpt(ctx);
		if (unlocked.empty()) query::throwFailed();
		return unlocked.value();
	}
}
