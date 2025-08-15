#pragma once

#include "../config.hpp"

#include <base/ints.hpp>
#include <base/raw_view.hpp>

template<typename T>
[[gnu::always_inline]]
inline static T& derefStack(std::byte* stack, u64 position) {
	return *(reinterpret_cast<T*>(&stack[position]));
}

template<typename T>
inline static T& derefView(base::ModRawView view) {
	return *(reinterpret_cast<T*>(view.getBegin()));
}

/**
 * Returns a reference (Ref) to the block corresponding to global data with id ID.
 * Should be preferred over DEREF_GLOBAL_RAW_UNSAFE in general.
 */
#define DEREF_GLOBAL(ID) thread.process_memory.getGlobalData(GlobalDataID(usize(ID)))

/**
 * Returns a reference of type TYPE (eg. int, i64, usize. etc) to a global data with id ID.
 */
#define DEREF_GLOBAL_RAW_UNSAFE(TYPE, ID) \
	derefView<TYPE>(thread.process_memory.getGlobalViewUnsafe(GlobalDataID(usize(ID))))

#if defined(__clang__) && __clang__ >= 13
	#define MUST_TAIL [[clang::musttail]]
#elif defined(__GNUG__) && __GNUG__ >= 15
	#define MUST_TAIL [[gnu::musttail]]
#else
	#define MUST_TAIL

	#ifdef USE_TAIL_CALLS
		#warning \
			"USE_TAIL_CALLS without support from compiler. This can potentially cause stack-overflow."
	#endif
#endif

/**
 * @brief Execute next instruction of the bytecode.
 * @param i indicates that the i-th next instruction will be executed,
 * with `0` being the current instruction.
 */
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
#define OPFUN_CONT(i)                                                                     \
	IF_TC({ MUST_TAIL return instr[i].tc_opfun(&instr[i], local_stack, frame, thread); }) \
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
		return instr[i].tc_opfun(&instr[i], local_stack, frame, thread);              \
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
