#include "access.hpp"

#include <query_framework/query_input.hpp>

namespace pst::detail {

	struct PSTAccessKey {
		PstID pst_id;

		PSTAccessKey(PstID pst_id): pst_id(pst_id) {}

		[[nodiscard]]
		u64 customPerfectHash() const { return pst_id.asInt(); }
	};

	DECLARE_QUERY_SIDE_INPUT(PSTAccessSideInput, PSTAccessKey);
	IMPLEMENT_QUERY_SIDE_INPUT(PSTAccessSideInput);

	void notifyContext(query::Context& ctx, PstID id) { ctx.query<PSTAccessSideInput>({id}); }

	void notifyBadAccess(query::Context&) { CORE_PANIC("BAD ACCESS"); }
}
