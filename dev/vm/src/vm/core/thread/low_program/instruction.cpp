#include "instruction.hpp"

#include <vm/config.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <vm/core/thread/opcode_functions/opcodes_functions.hpp>

namespace vm {

	Fix8Instruction makeLowInstruction(u64 opcode, i32 arg0, i32 arg1) {
#ifdef USE_TAIL_CALLS
		return Fix8Instruction{
			.opfun = OpFuns::OPFUNS.at(opcode),
			.arg0  = arg0,
			.arg1  = arg1,
		};
#else
		return Fix8Instruction{
			.opcode = opcode,
			.arg0   = arg0,
			.arg1   = arg1,
		};
#endif
	}
}
