#pragma once

#include <config.hpp>

#if defined(__clang__)
	#define CLANG_MUST_TAIL [[clang::musttail]]
#else
	#define CLANG_MUST_TAIL
#endif

#define LABEL_PTR(opcode) (&&LABEL_##opcode)

/**
 * @brief Jump to next bytecode instruction in CG style main loop.
 */
#define DISPATCH_OPCODE()                                                     \
	{                                                                         \
		/* NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)) */ \
		goto* opcode_label[static_cast<u64>(instr->opcode)];                  \
		/* NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)) */   \
	}

#define OP_CASE_HEADER(opcode)  case OpcodeFix8 ::opcode:
#define OP_LABEL_HEADER(opcode) LABEL_##opcode:

/**
 * @brief Define a case for an opcode in swith-case for SC variant,
 * or CG instruction dispatch label for CG variant. After opcode execution,
 * continues the execution loop.
 */
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

/**
 * @brief Same as #OP_CASE, but terminates the execution loop instead of
 * continuing.
 */
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

/**
 * @brief Execute next instruction of the bytecode.
 * @param i indicates that the i-th next instruction will be executed,
 * with `0` being the current instruction.
 */
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
#define OPFUN_CONT(i)                                                                        \
	IF_TC({ CLANG_MUST_TAIL return instr[i].opfun(&instr[i], local_stack, frame, thread); }) \
	IF_NOT_TC({ instr += i; })
// NOLINTEND(cppcoreguidelines-pro-type-union-access)

/**
 * @brief Same as #OPFUN_CONT, but this also handles execution strategy check.
 */
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
#define OPFUN_CONT_CHECK_STRATEGY(i)                                              \
	IF_TC({                                                                       \
		if constexpr (!IGNORE_EXECUTION_STRATEGY) {                               \
			if (!thread.is_running)                                               \
				return op_handle_strategy(&instr[i], local_stack, frame, thread); \
		}                                                                         \
		return instr[i].opfun(&instr[i], local_stack, frame, thread);             \
	})                                                                            \
	IF_NOT_TC({                                                                   \
		instr += i;                                                               \
		if constexpr (!IGNORE_EXECUTION_STRATEGY) {                               \
			if (!thread.is_running) [[unlikely]] {                                \
				return op_handle_strategy(instr, local_stack, frame, thread);     \
			}                                                                     \
		}                                                                         \
	})
// NOLINTEND(cppcoreguidelines-pro-type-union-access)

// Prevent \ warning
