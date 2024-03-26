#pragma once

#include "instruction.hpp"
#include <vector>
#include <string>

namespace vm {

	using ByteCode = std::vector<struct Fix8Instruction>;

	struct FuncData {
		ByteCode bc;
		usize    stack_size;
		usize    arg_size;
		usize    ret_size;
	};

	struct Code {
		std::vector<FuncData> functions;
		usize                 main_id;
	};
}
