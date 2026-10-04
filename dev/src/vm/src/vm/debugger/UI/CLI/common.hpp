#pragma once

#include <diagnostic/highlight_positions.hpp>

#include <vm/api/api.hpp>

#include <string>
#include <vector>

namespace vm::debugger::cli::common {
	/**
	 * @brief erases spaces, tabs, newlines and CR from both ends of a string
	 */
	std::string strip(const std::string& string);

	/**
	 * @brief extracts primitive values from the status if available
	 */
	std::vector<std::string> extractPrimitiveValues(const vm::api::ProcStatus& status);

	/**
	 * @brief returns a string representation of the status and its primitive values if available
	 */
	std::string statusLine(const vm::api::ProcStatus& status);

	/**
	 * @brief removes control sequences from a string
	 */
	std::string withoutControlSequences(const std::string& original);
}
