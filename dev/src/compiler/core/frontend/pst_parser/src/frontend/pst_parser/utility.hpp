#pragma once

#include <diagnostic/source_position.hpp>

namespace pst::internal {
	void printHighlight(dia::SourcePosition pos, const std::string& message);
}
