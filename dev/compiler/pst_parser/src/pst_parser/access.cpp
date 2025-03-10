#include "access.hpp"
#include <query_framework/query_impl.hpp>

namespace pst::detail {
	void notifyContext(query::Context& ctx) { ctx.setSidePSTInput(); }
}
