/**
 * @file code.hpp
 */
#pragma once

#include "instruction.hpp"
#include <vector>

namespace vm {
	using ByteCode = std::vector<Fix8Instruction>;

	/**
	 * @brief Function data.
	 */
	struct FuncData {
		ByteCode bc;
		usize    stack_size;
		usize    arg_size;
		usize    next_arg_size;
		usize    ret_size;
	};

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
