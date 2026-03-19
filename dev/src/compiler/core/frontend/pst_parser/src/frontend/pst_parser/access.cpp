#include "access.hpp"

#include "pst_query/pst_access_side_input.hpp"

#include <diagnostic_interactive/placeholder.hpp>

#include <query_framework/input_query/query_input_impl.hpp>
#include <query_framework/query_errors.hpp>

namespace pst::internal {

	IMPLEMENT_QUERY_SIDE_INPUT(PSTAccessSideInput);

	void notifyContext(query::Context& ctx, query::QueryStableHash stable_hash) {
		ctx.query<PSTAccessSideInput>({ stable_hash });
	}

	void notifyBadAccess(query::Context& ctx) {
		ctx.logInt(makeBox<dia_int::PlaceholderHeaderError>(
			"PST Accessed a nullptr LangElement.",
			"To check the location of the bad access, enable "
			"query dev logs."
		));
		query::throwFailed();
	}
}
