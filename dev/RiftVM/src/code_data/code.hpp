#pragma once

#include "instruction.hpp"
#include <string>
#include <vector>

namespace vm {

	typedef std::vector<Fix8Instruction> ByteCode;

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
