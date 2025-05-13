#include "code_dependency.hpp"

#include "pst_access_side_input.hpp"

#include <query_framework/context.hpp>

namespace pst {
	std::vector<dia::SourcePosition> queryPositionDependencies(
		query::Context& ctx, query::detail::NodeID id
	) {
		auto nodes = ctx.getGraph().getNodeDepsFiltered(id, detail::PSTAccessSideInput::getID());
	}
}
