// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/except/exceptions.hpp>

#include <query_framework/input_query/query_input.hpp>
#include <query_framework/utils/query_hash.hpp>

namespace pst::internal {
	struct PSTAccessKey final {
		query::QueryStableHash hash;

		PSTAccessKey(query::QueryStableHash stable_hash): hash(stable_hash) {}

		[[nodiscard]]
		query::QueryStableHash queryStablePerfectHash() const {
			return hash;
		}
	};

	/**
	 * @brief Query that is used as a PST-access side input.
	 * @note it is in hpp only to be able to get its QueryID.
	 */
	DECLARE_QUERY_SIDE_INPUT(PSTAccessSideInput, PSTAccessKey);
}
