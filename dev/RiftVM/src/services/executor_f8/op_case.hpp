#pragma once

#include <code_data/opcodes.hpp>

#include "op_case_config.hpp"

#define FRAME(arg)       \
	IF_NOT_FF(frame.arg) \
	IF_FF(arg)

#define FRAME_FLAGS(arg)       \
	IF_NOT_FF(frame.flags.arg) \
	IF_FF(arg)

#define FRAME_REGS(arg)       \
	IF_NOT_FF(frame.regs.arg) \
	IF_FF(arg)


#define LABEL_PTR(opcode) (&&LABEL_##opcode)

#define DISPATCH_OPCODE()                                                            \
	{                                                                                \
		nextInstruction(FRAME(bc), FRAME(instruction_pointer), opcode, arg0, arg1);  \
		if constexpr (!IGNORE_EXECUTION_STRATEGY) handleExecutionStrategyIfNeeded(); \
		goto* opcode_label[static_cast<u64>(opcode)];                                \
	}

#define OP_CASE_HEADER(opcode)  case OpcodeFix8 ::opcode:
#define OP_LABEL_HEADER(opcode) LABEL_##opcode:

#define OP_CASE(opcode, body)                                                             \
	IF_NOT_CG(OP_CASE_HEADER(opcode))                                                     \
	IF_CG(OP_LABEL_HEADER(opcode)) {                                                      \
		IF_CG(static_assert(                                                              \
				  opcode_label[static_cast<u64>(OpcodeFix8::opcode)] == LABEL_PTR(opcode) \
		););                                                                              \
		{                                                                                 \
			{ body }                                                                      \
			{                                                                             \
				IF_CG(DISPATCH_OPCODE())                                                  \
				IF_NOT_CG(break;)                                                         \
			}                                                                             \
		}                                                                                 \
	}


// Prevent \ warning
