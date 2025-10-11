#pragma once

#include <vm/bytecode/opcode_args.hpp>

#include <tuple>

namespace vm::low::instructions {

#define HANDLE_MICRO_INSTR_0ARGS(instr) \
	struct instr {                      \
		using Args = std::tuple<>;      \
	};
#define HANDLE_MICRO_INSTR_1ARGS(instr, arg0) \
	struct instr {                            \
		using Args = std::tuple<arg0>;        \
	};
#define HANDLE_MICRO_INSTR_2ARGS(instr, arg0, arg1) \
	struct instr {                                  \
		using Args = std::tuple<arg0, arg1>;        \
	};
#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_2ARGS

}
