#pragma once

#include "config.hpp"

#define LABEL_PTR(opcode) (&&LABEL_##opcode)

#define DISPATCH_OPCODE()                                                     \
	{                                                                         \
		/* NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)) */ \
		goto* opcode_label[static_cast<u64>(instr->opcode)];                  \
		/* NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)) */   \
	}

#define OP_CASE_HEADER(opcode)  case OpcodeFix8 ::opcode:
#define OP_LABEL_HEADER(opcode) LABEL_##opcode:

#define OP_CASE(opcode)                                                \
	IF_NOT_CG(OP_CASE_HEADER(opcode))                                  \
	IF_CG(OP_LABEL_HEADER(opcode)) {                                   \
		{                                                              \
			vm::OpFuns::op_##opcode(instr, local_stack, frame, *this); \
			{                                                          \
				IF_CG(DISPATCH_OPCODE())                               \
				IF_NOT_CG(break;)                                      \
			}                                                          \
		}                                                              \
	}

#define OP_CASE_END(opcode)                                            \
	IF_NOT_CG(OP_CASE_HEADER(opcode))                                  \
	IF_CG(OP_LABEL_HEADER(opcode)) {                                   \
		{                                                              \
			vm::OpFuns::op_##opcode(instr, local_stack, frame, *this); \
			/* NOLINTBEGIN(cppcoreguidelines-avoid-goto) */            \
			goto End;                                                  \
			/* NOLINTEND(cppcoreguidelines-avoid-goto) */              \
		}                                                              \
	}


// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
#define OPFUN_CONT(i)                                                          \
	IF_TC({ return instr[i].opfun(&instr[i], local_stack, frame, executor); }) \
	IF_NOT_TC({ instr += i; })
// NOLINTEND(cppcoreguidelines-pro-type-union-access)

// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
#define OPFUN_CONT_CHECK_STRATEGY(i)                                                \
	IF_TC({                                                                         \
		if constexpr (!IGNORE_EXECUTION_STRATEGY) {                                 \
			if (!executor.is_running)                                               \
				return op_handle_strategy(&instr[i], local_stack, frame, executor); \
		}                                                                           \
		return instr[i].opfun(&instr[i], local_stack, frame, executor);             \
	})                                                                              \
	IF_NOT_TC({                                                                     \
		instr += i;                                                                 \
		if constexpr (!IGNORE_EXECUTION_STRATEGY) {                                 \
			if (!executor.is_running) [[unlikely]] {                                \
				return op_handle_strategy(instr, local_stack, frame, executor);     \
			}                                                                       \
		}                                                                           \
	})
// NOLINTEND(cppcoreguidelines-pro-type-union-access)

// Prevent \ warning
