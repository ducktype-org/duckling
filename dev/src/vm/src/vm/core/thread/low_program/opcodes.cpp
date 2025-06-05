#include "opcodes.hpp"

namespace vm::low {

	template<class Instr>
	struct InstrToOpcodeFix8;

	template<>
	struct InstrToOpcodeFix8<code::instructions::Comment> {
		static constexpr OpcodeFix8 OPCODE_FIX8 = OpcodeFix8::Comment;
	};

#define HANDLE_OPCODE(opcode)                                         \
	template<>                                                        \
	struct InstrToOpcodeFix8<VM_INSTR_FROM_NAME(opcode)> {            \
		static constexpr OpcodeFix8 OPCODE_FIX8 = OpcodeFix8::opcode; \
	};

#include <vm/bytecode/opcode_definitions.hpp>
#undef HANDLE_OPCODE

	u16 fix8FromInstr(const code::Instruction& instruction) {
		return u16(
			VISIT(instruction,
		          var,
		          return low::InstrToOpcodeFix8<std::remove_cvref_t<decltype(var)>>::OPCODE_FIX8;)
		);
	}
}
