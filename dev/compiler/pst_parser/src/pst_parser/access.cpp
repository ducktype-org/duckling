#include "access.hpp"
#include <query_framework/query_impl.hpp>

namespace pst::detail {
	void notifyContext(query::detail::ContextType& ctx) { ctx.setSidePSTInput(); }
}
