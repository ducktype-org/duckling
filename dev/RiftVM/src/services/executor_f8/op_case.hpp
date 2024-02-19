#pragma once

#include "config.hpp"

#define FRAME(arg)       \
	IF_NOT_FF(frame.arg) \
	IF_FF(arg)

#define EXECUTOR(arg) FRAME(executor).arg

#define FRAME_FLAGS(arg)       \
	IF_NOT_FF(frame.flags.arg) \
	IF_FF(arg)

#define FRAME_REGS(arg)       \
	IF_NOT_FF(frame.regs.arg) \
	IF_FF(arg)


#define LABEL_PTR(opcode) (&&LABEL_##opcode)

#define DISPATCH_OPCODE()                                                                   \
	{                                                                                       \
		if constexpr (!IGNORE_EXECUTION_STRATEGY)                                           \
			FRAME(executor).handleExecutionStrategyIfNeeded();                              \
		goto* opcode_label[static_cast<u64>(FRAME(bc)[FRAME(instruction_pointer)].opcode)]; \
	}

#define OP_CASE_HEADER(opcode)  case OpcodeFix8 ::opcode:
#define OP_LABEL_HEADER(opcode) LABEL_##opcode:

#define OP_CASE(opcode)                                                              \
	IF_NOT_CG(OP_CASE_HEADER(opcode))                                                \
	IF_CG(OP_LABEL_HEADER(opcode)) {                                                 \
		{                                                                            \
			const Fix8Instruction* instr = &FRAME(bc)[FRAME(instruction_pointer)++]; \
			vm::OpFuns::op_##opcode(instr, r1, r2, r3, local_stack, frame);          \
			{                                                                        \
				IF_CG(DISPATCH_OPCODE())                                             \
				IF_NOT_CG(break;)                                                    \
			}                                                                        \
		}                                                                            \
	}

#define OP_CASE_END(opcode)                                                          \
	IF_NOT_CG(OP_CASE_HEADER(opcode))                                                \
	IF_CG(OP_LABEL_HEADER(opcode)) {                                                 \
		{                                                                            \
			const Fix8Instruction* instr = &FRAME(bc)[FRAME(instruction_pointer)++]; \
			vm::OpFuns::op_##opcode(instr, r1, r2, r3, local_stack, frame);          \
			goto End;                                                                \
		}                                                                            \
	}

// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
#define OPFUN_CONT(i, r1, r2, r3)                                                         \
	IF_TC({                                                                               \
		if constexpr (!IGNORE_EXECUTION_STRATEGY) {                                       \
			if (!FRAME(executor).isRunning)                                               \
				return op_handle_strategy(&instr[i - 1], r1, r2, r3, local_stack, frame); \
		}                                                                                 \
		return instr[i].opfun(&instr[i], r1, r2, r3, local_stack, frame);                 \
	})
// NOLINTEND(cppcoreguidelines-pro-type-union-access)

// Prevent \ warning
