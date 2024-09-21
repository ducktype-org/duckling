#pragma once

#include "instruction.hpp"
#include <vector>

namespace vm {
	using ByteCode = std::vector<Fix8Instruction>;

	struct FuncData {
		ByteCode bc;
		usize    stack_size;
		usize    arg_size;
		usize    next_arg_size;
		usize    ret_size;
	};

	struct Code {
		std::vector<FuncData> functions;
		usize                 main_id;
	};
}
