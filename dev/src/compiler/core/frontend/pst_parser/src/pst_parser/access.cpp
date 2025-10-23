#include "access.hpp"

#include "pst_query/pst_access_side_input.hpp"

#include <query_framework/query_input_impl.hpp>

namespace pst::internal {

	IMPLEMENT_QUERY_SIDE_INPUT(PSTAccessSideInput);

	void notifyContext(query::Context& ctx, query::QueryStableHash stable_hash) {
		ctx.query<PSTAccessSideInput>({ stable_hash });
	}

	void notifyBadAccess(query::Context&) { CORE_PANIC("PST-Access to a nullptr."); }
}
