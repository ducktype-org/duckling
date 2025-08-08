#pragma once

#include "../config.hpp"

#include <base/ints.hpp>
#include <base/raw_view.hpp>

#include "vm/core/thread/vmthread.hpp"
#include <vm/utils/interpret.hpp>

template<typename T>
[[nodiscard]] [[gnu::always_inline]]
inline static T readFromStack(std::byte* stack, u64 position) {
	return vm::safeReadBytes<T>(stack, position);
}

template<typename T>
[[gnu::always_inline]]
inline static void writeToStack(std::byte* stack, u64 position, const T& value) {
	return vm::safeWriteBytes<T>(stack, value, position);
}

template<typename T>
[[nodiscard]] [[gnu::always_inline]]
inline static T readFromView(base::ModRawView view) {
	return vm::safeReadBytes<T>(view.getBegin());
}

template<typename T>
[[gnu::always_inline]]
inline static void writeToView(base::ModRawView view, const T& value) {
	return vm::safeWriteBytes<T>(view.getBegin(), value);
}

// TODOP: Temporary, remove that.
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
 * @brief Reads a value of a given TYPE from a global memory location specified by a global ID.
 */
#define READ_FROM_GLOBAL_VIEW_BEGIN(TYPE, GLOBAL_ID) \
	thread.process_memory.getGlobalViewUnsafe(GlobalDataID(usize(GLOBAL_ID))).getBegin()

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


/**
 * Returns a reference of type TYPE (eg. int, i64, usize. etc) to a global data with id ID.
 */
#define DEREF_GLOBAL_RAW_UNSAFE(TYPE, ID) \
	derefView<TYPE>(thread.process_memory.getGlobalViewUnsafe(GlobalDataID(usize(ID))))

/**
 * Returns a reference (Ref) to the block corresponding to global data with id ID.
 * Should be preferred over DEREF_GLOBAL_RAW_UNSAFE in general.
 */
#define GET_GLOBAL_BLOCK(ID) thread.process_memory.getGlobalData(GlobalDataID(usize(ID)))

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
#define OPFUN_CONT(i)                                                                           \
	IF_TC({ CLANG_MUST_TAIL return instr[i].tc_opfun(&instr[i], local_stack, frame, thread); }) \
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
