#include "opcodes.hpp"

namespace vm::low {

	template<class Instr>
	struct MicroInstrToOpcode;

#define HANDLE_MICRO_INSTR(instr)                                    \
	template<>                                                        \
	struct MicroInstrToOpcode<VM_INSTR_FROM_NAME(instr)> {            \
		static constexpr MicroOpcode OPCODE = MicroOpcode::instr; \
	};

#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR

    // @TODOB is this needed where we're going?
	u64 fix8FromMicroInstr(const code::Instruction& instruction) {
        CORE_PANIC("trying to get fix8");
	}
}
