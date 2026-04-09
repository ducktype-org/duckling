#pragma once

#include "../config.hpp"

#include <base/misc/raw_view.hpp>
#include <base/types/ints.hpp>
#include <base/pointers/ref.hpp>

#include <vm/utils/interpret.hpp>
struct Block;

/**
 * @brief Writes a value of a given TYPE to a specified location on the stack.
 */
template<typename T>
[[nodiscard]] [[gnu::always_inline]]
inline static T readFromPlace(std::byte* local_stack, std::byte* global_buffer, u64 place_arg) {
	uint64_t offset = place_arg & 0x7FFFFFFFFFFFFFFF;
	uint64_t on_bit = place_arg >> 63;
	auto address = (std::byte*)(((uint64_t)global_buffer * on_bit) + ((uint64_t)local_stack * !on_bit) + offset); //NOLINT
	return vm::safeReadPointerBytes<T>(address);
}

/**
 * @brief Writes a value of a given TYPE to a specified location on the stack.
 */
template<typename T>
[[gnu::always_inline]]
inline static void writeToPlace(std::byte* local_stack, std::byte* global_buffer, u64 place_arg, const T& value) {
	uint64_t offset = place_arg & 0x7FFFFFFFFFFFFFFF;
	uint64_t on_bit = place_arg >> 63;
	auto address = (std::byte*)(((uint64_t)global_buffer * on_bit) + ((uint64_t)local_stack * !on_bit) + offset); //NOLINT
	return vm::safeWriteBytes<T>(address, value);
}

[[nodiscard]] [[gnu::always_inline]]
inline static Ref<Block> getBlockRefFromArg(Block** local_stack_blocks, Block** global_buffer_blocks, u64 place_arg) {
	uint64_t offset = place_arg & 0x7FFFFFFFFFFFFFFF;
	uint64_t on_bit = place_arg >> 63;
	auto address = (Block**)(((uint64_t)global_buffer_blocks * on_bit) + ((uint64_t)local_stack_blocks * !on_bit) + offset); //NOLINT
	return {*address};
}


#define READ_FROM_PLACE_ARG(TYPE, ARG) readFromPlace<TYPE>(local_stack, thread.runtime_data.global_data_buffer_base, ARG)
#define WRITE_TO_PLACE_ARG(TYPE, ARG, VALUE) writeToPlace<TYPE>(local_stack, thread.runtime_data.global_data_buffer_base, ARG, VALUE)
#define READ_BLOCK_REF_FROM_ARG(ARG) getBlockRefFromArg(frame->local_block_ref_stack_base, thread.runtime_data.global_block_ref_buffer_base, ARG)

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
			if (thread.getExecutionRequestPendingFlag())                              \
				return handle_execution_break(&instr[i], local_stack, frame, thread); \
		}                                                                             \
		MUST_TAIL return instr[i].tc_opfun(&instr[i], local_stack, frame, thread);    \
	})                                                                                \
	IF_NOT_TC({                                                                       \
		instr += i;                                                                   \
		if constexpr (!IGNORE_EXECUTION_STRATEGY) {                                   \
			if (thread.getExecutionRequestPendingFlag()) [[unlikely]] {               \
				return handle_execution_break(instr, local_stack, frame, thread);     \
			}                                                                         \
		}                                                                             \
	})
// NOLINTEND(cppcoreguidelines-pro-type-union-access)

// Prevent \ warning
