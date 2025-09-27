#include "instruction.hpp"

#include <vm/core/thread/opcode_functions/opcodes_functions.hpp>

namespace vm {

	MicroInstruction makeLowInstruction(u64 opcode, u64 arg0, u64 arg1) {
#ifdef USE_TAIL_CALLS
		return MicroInstruction{
			.tc_opfun = OpFuns::OPFUNS.at(opcode),
			.arg0     = arg0,
			.arg1     = arg1,
		};
#else
		return MicroInstruction{
			.nontc_opcode = opcode,
			.arg0         = arg0,
			.arg1         = arg1,
		};
#endif
	}

	std::string getInstructionConfig() {
#ifdef USE_SWITCH_CASE
		return "Switch case";
#endif
#ifdef USE_COMPUTED_GOTO
		return "Computed goto";
#endif
#ifdef USE_TAIL_CALLS
		return "Tail calls";
#endif
	}
}
