#include "instruction.hpp"

#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>

namespace vm {

	MicroInstruction makeLowInstruction(low::MicroOpcode opcode, u64 arg0, u64 arg1) {
		u64 opcode_num = std::to_underlying(opcode);
#ifdef USE_TAIL_CALLS
		return MicroInstruction{
			.tc_opfun = OpFuns::OPFUNS.at(opcode_num),
			.arg0     = arg0,
			.arg1     = arg1,
		};
#else
		return MicroInstruction{
			.nontc_opcode = opcode_num,
			.arg0         = arg0,
			.arg1         = arg1,
		};
#endif
	}

	void setBreakpoint(MicroInstruction& instruction, bool enable) {
		auto prev_opcode = getInstructionOpcode(instruction);
		auto new_opcode
			= enable ? low::breakpointVersion(prev_opcode) : low::getUnderlying(prev_opcode);

		instruction = makeLowInstruction(new_opcode, instruction.arg0, instruction.arg1);
	}

	low::MicroOpcode getInstructionOpcode(const MicroInstruction& instruction) {
#ifdef USE_TAIL_CALLS
		return OpFuns::getOpcodeFromOpFun(instruction.tc_opfun);
#else
		return low::MicroOpcode{ instruction.nontc_opcode };
#endif
	}

	std::string getInstructionConfig() {
#ifdef USE_SWITCH_CASE
		return "Switch case";
#endif
#ifdef USE_TAIL_CALLS
		return "Tail calls";
#endif
	}
}
