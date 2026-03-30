#pragma once

#include "../config.hpp"

#include <base/misc/raw_view.hpp>
#include <base/types/ints.hpp>

#include <vm/utils/interpret.hpp>

/**
 * @brief Reads a value of a given TYPE from the specified location on the stack.
 */
template<typename T>
[[nodiscard]] [[gnu::always_inline]]
inline static T readFromStack(std::byte* stack, u64 position) {
	return vm::safeReadPointerBytes<T>(stack, position);
}

/**
 * @brief Writes a value of a given TYPE to a specified location on the stack.
 */
template<typename T>
[[gnu::always_inline]]
inline static void writeToStack(std::byte* stack, u64 position, const T& value) {
	return vm::safeWriteBytes<T>(stack, value, position);
}

/**
 * @brief Reads a value of a given TYPE from the beginning of the given view.
 */
template<typename T>
[[nodiscard]] [[gnu::always_inline]]
inline static T readFromView(base::ModRawView view) {
	return vm::safeReadPointerBytes<T>(view.getBegin());
}

/**
 * @brief Writes a value of a given TYPE to the beginning of the given view.
 */
template<typename T>
[[gnu::always_inline]]
inline static void writeToView(base::ModRawView view, const T& value) {
	return vm::safeWriteBytes<T>(view.getBegin(), value);
}

/**
 * @brief Returns a block containing the data of the global specified by the ID.
 */
#define GET_GLOBAL_BLOCK(ID) thread.process_memory.getGlobalData(GlobalDataID(usize(ID)))

/**
 * @brief Reads a value of a given TYPE from a global memory location specified by a global ID.
 */
#define READ_FROM_GLOBAL(TYPE, GLOBAL_ID)                                               \
	([&](u64 id) {                                                                      \
		auto view = thread.process_memory.getGlobalViewUnsafe(GlobalDataID(usize(id))); \
		return readFromView<TYPE>(view);                                                \
	}(GLOBAL_ID))

/**
 * @brief Writes a value to a global memory location specified by a global ID.
 */
#define WRITE_TO_GLOBAL(TYPE, GLOBAL_ID, VALUE)                                                \
	do {                                                                                       \
		auto view = thread.process_memory.getGlobalViewUnsafe(GlobalDataID(usize(GLOBAL_ID))); \
		writeToView<TYPE>(view, VALUE);                                                        \
	} while (false)

#if defined(__clang_major__) && __clang_major__ >= 13
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
		MUST_TAIL return instr[i].tc_opfun(&instr[i], local_stack, frame, thread);    \
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
