#pragma once

namespace pst {
	/**
	 * @brief Type of ordering in a code block and top level
	 *
	 * Relevant for some behaviors, for example path to node as different blocks can be ordered
	 * like in a function or more unordered like in the global scope. Undefined is just a
	 * default that will cause an error if another type is not set.
	 *
	 * Unordered - Statements that declare the different symbols, statements that don't declare
	 * symbols and transparent statements have separate orders. Ordered - Order of statements is
	 * as one list. Undefined - Illegal default state.
	 */
	enum class BlockOrderType {
		Unordered,
		Ordered,
		Undefined,
	};

	/**
	 * @brief Current statement context, used to choose Statement choice
	 *
	 */
	enum class StmtContext {
		Normal,
		Class,
		Undefined,
	};
}
