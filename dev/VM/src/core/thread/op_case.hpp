#pragma once

#include <config.hpp>

#if defined(__clang__)
	#define CLANG_MUST_TAIL [[clang::musttail]]
#else
	#define CLANG_MUST_TAIL  //@todo in the newest GCC version there is a musttail attribute
#endif

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
#define OPFUN_CONT_CHECK_STRATEGY(i)                                                  \
	IF_TC({                                                                           \
		if constexpr (!IGNORE_EXECUTION_STRATEGY) {                                   \
			if (thread.execution_request_break)                                       \
				return handle_execution_break(&instr[i], local_stack, frame, thread); \
		}                                                                             \
		return instr[i].opfun(&instr[i], local_stack, frame, thread);                 \
	})                                                                                \
	IF_NOT_TC({                                                                       \
		instr += i;                                                                   \
		if constexpr (!IGNORE_EXECUTION_STRATEGY) {                                   \
			if (thread.execution_request_break) [[unlikely]] {                        \
				return handle_execution_break(instr, local_stack, frame, thread);     \
			}                                                                         \
		}                                                                             \
	})
// NOLINTEND(cppcoreguidelines-pro-type-union-access)

// Prevent \ warning
