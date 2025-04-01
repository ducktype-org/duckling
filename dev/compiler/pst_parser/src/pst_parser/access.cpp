#include "access.hpp"

#include <query_framework/context.hpp>

namespace pst::detail {
	void notifyContext(query::Context& ctx) { ctx.setSidePSTInput(); }

	void notifyBadAccess(query::Context&) { CORE_PANIC("BAD ACCESS"); }
}
