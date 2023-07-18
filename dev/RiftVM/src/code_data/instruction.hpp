#pragma once

#include <cstdint>
#include "opcodes.hpp"

#define USE_COMPACT_INSTRUCTION

namespace vm {

	#ifdef USE_COMPACT_INSTRUCTION
		struct Fix8Instruction {
			int64_t opcode: 16, arg0: 24, arg1: 24;
		};
	#else
		struct Fix8Instruction {
			uint16_t opcode;
			int32_t arg0;
			int32_t arg1;
		};
	#endif

}
