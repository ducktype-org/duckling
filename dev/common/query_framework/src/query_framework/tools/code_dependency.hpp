#pragma once

#include <diagnostic/source_position.hpp>

namespace query {
	/**
	 * @brief Returns the minimal set of Source Positions that contain the dependencies of a given query.
	 */
	std::vector<dia::SourcePosition> queryPositionDependencies();
}