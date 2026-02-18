#pragma once

#include "../mir_structure/mir_structure.hpp"

namespace compiler::mir {

	/**
	 * @brief Validates function, currently checks whether moves are used correctly
	 * and whether local variables are not being shadowed.
	 * @TODO #858 when move flag will be set, write proper tests.
	 */
	base::OkBad validateFunction(query::Context& ctx, const Function&);
}
