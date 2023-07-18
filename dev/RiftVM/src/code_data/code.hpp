#pragma once

#include "instruction.hpp"
#include <vector>
#include <string>

namespace vm {

	typedef std::vector<Fix8Instruction> ByteCode;

	struct FuncData {
		ByteCode bc;
		size_t stack_size;
		size_t arg_size;
		size_t ret_size;
	};

	struct Code {
		std::vector<FuncData> functions;
		size_t main_id;
	};
}





