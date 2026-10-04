#pragma once

#include "../mir_structure/mir_structure.hpp"

namespace compiler::mir {

	/**
	 * @brief Validates function whether local variables are not being shadowed.
	 */
	base::OkBad validateFunction(query::Context& ctx, const Function&);
}
