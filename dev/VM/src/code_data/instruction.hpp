#pragma once

#include "frame.hpp"
#include "config.hpp"
#include <base/ints.hpp>
#include <array>

// #define USE_COMPACT_INSTRUCTION

#define OPFUN_ARGS                                                    \
	const Fix8Instruction *IF_NOT_TC(&) instr [[maybe_unused]],       \
		std::byte *        IF_NOT_TC(&) local_stack [[maybe_unused]], \
		Frame *IF_NOT_TC(&) frame [[maybe_unused]], Executor &executor [[maybe_unused]]

#define RETURN_TYPE IF_NOT_TC([[gnu::always_inline]] inline) void

namespace vm {
	class Executor;
	struct Fix8Instruction;

	class OpFuns;
	using OpFun = void(OPFUN_ARGS);

	// Describes number of DucklingBC opcodes + meta-opcodes recognized by Executor.
	// This constant is relevant for `vm::Opfuns::opfuns[]` (instructions.hpp) and `opcode_label[]`
	// (CG, executor.cpp)
	constexpr u16 OP_CASES_COUNT = 40;

#ifdef USE_TAIL_CALLS
	struct Fix8Instruction {
		OpFun* opfun;
		i32    arg0;
		i32    arg1;
	};
#else
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
#endif

	class OpFuns {
	public:
#define DEF_OPCODE(opcode) static OpFun op_##opcode;
#include "opcodes_list.hpp"
#undef DEF_OPCODE

		// A mapping between opcode ids and function pointers.
		// WARN: Ordering of elements must stay the same as in vm::OpcodeFix8
		static constexpr std::array<OpFun*, OP_CASES_COUNT> opfuns{
#define DEF_OPCODE(opcode) op_##opcode,
#include "opcodes_list.hpp"
#undef DEF_OPCODE
		};
	};
}  // namespace vm
