#pragma once

#include <query_framework/context/context_fd.hpp>

namespace compiler::mir {
	struct Function;

	/**
	 * @brief Perform a pass of MIR, that adds destructor calls
	 * based on instruction lifetime-scopes.
	 *
	 * @important
	 * It is a mock implementation, and does not perform
	 * lifetime checks. This means that it will add destructors for all locals, even if they
	 * are not yet created. example:
	 * ```cpp
	 * fun foo() { return; var a: T; } // calls destructor on return
	 * ```
	 *
	 * @todo make it a final implementation, that takes liveness into account.
	 * Ideas:
	 *   * "add liveness" after adding destructors, and delete destructors that are not needed.
	 *   * make explicit cfg graph, and somehow walk it to find liveness ranges.
	 *   * somehow use lifetime_scopes to find liveness ranges.
	 *
	 * @todo Currently no code is put in place to
	 * generate correct order of destructors calls.
	 *
	 * @todo Add better tests once its not mock anymore.
	 *
	 * @return Function
	 */
	Function addDestructors(query::Context&, Function);
}
