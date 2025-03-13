#include "access.hpp"
#include <query_framework/query_impl.hpp>

namespace pst::detail {
	void notifyContext(query::Context& ctx) { ctx.setSidePSTInput(); }

	void notifyBadAccess(query::detail::ContextType&) { CORE_PANIC("BAD ACCESS"); }
}
