#pragma once

// @TODO: this should be able to only import Import:
#include <pst_parser/elements/elements.hpp>

/**
 * This is meant as a placeholder.
 */

namespace modulesys {
	std::string importToPath(std::string_view local_path, const pst::Import& import);
}
