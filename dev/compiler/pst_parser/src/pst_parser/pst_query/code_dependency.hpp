#pragma once

#include <diagnostic/source_position.hpp>
#include <query_framework/detail/query_graph/query_graph.hpp>

namespace pst {
	/**
	 * @brief Returns the minimal set of Source Positions that contain the dependencies of a given
	 * query.
	 */
	std::vector<dia::SourcePosition> queryPositionDependencies(query::detail::NodeID);
}
