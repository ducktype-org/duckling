/**
 * @file code.hpp
 */
#pragma once

#include "instruction.hpp"
#include "program.hpp"
#include <vector>

namespace vm {
	/**
	 * @brief Representation of the whole code.
	 * Parser creates this structure from the text file and the Executor uses it to execute the
	 * code.
	 */
	struct Code {
		std::vector<FuncData> functions;
		usize                 main_id;
	};
}
