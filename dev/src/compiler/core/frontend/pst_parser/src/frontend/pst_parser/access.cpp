// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "access.hpp"

#include "pst_query/pst_access_side_input.hpp"

#include <diagnostic/placeholder.hpp>
#include <query_framework/input_query/query_input_impl.hpp>
#include <query_framework/query_errors.hpp>

namespace pst::internal {

	IMPLEMENT_QUERY_SIDE_INPUT(PSTAccessSideInput);

	void notifyContext(query::Context& ctx, query::QueryStableHash stable_hash) {
		ctx.query<PSTAccessSideInput>({ stable_hash });
	}

	void notifyBadAccess(query::Context& ctx) {
		ctx.logInt(makeBox<dia::PlaceholderError>(
			"PST Accessed a nullptr LangElement.",
			"To check the location of the bad access, enable "
			"query dev logs (Query, QueryStacktraces)."
		));
		query::throwFailed();
	}
}
