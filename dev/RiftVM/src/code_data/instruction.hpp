#pragma once

#include "opcodes.hpp"
#include <base/ints.hpp>

#define USE_COMPACT_INSTRUCTION

namespace vm {

#ifdef USE_COMPACT_INSTRUCTION
	struct Fix8Instruction {
		i64 opcode: 16, arg0: 24, arg1: 24;
	};
#else
	struct Fix8Instruction {
		u16 opcode;
		i32 arg0;
		i32 arg1;
	};
#endif

}
