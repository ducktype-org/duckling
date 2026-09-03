#pragma once

#include "../../config.hpp"

#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/musttail.hpp>
#include <vm/utils/interpret.hpp>

namespace vm {
	template<typename EntryT>
	class GenericBlock;
	using Block = GenericBlock<std::byte>;
}

/**
 * @brief Given the argument of type "place", which is an offset to the global buffer or local
 * stack, with the highest bit indicating whether it's global or local, returns the pointer to the
 * actual byte in memory.
 * @param arg - the mentioned argument
 */
[[gnu::always_inline]]
inline static std::byte* getBytePtrFromPlaceArg(
	std::byte* local_stack, std::byte* global_buffer, u64 arg
) {
	// Extract the highest bit.
	bool is_global = (arg >> 63) != 0;

	// Mask out the highest bit to get the offset.
	u64 offset = arg & ~(1ULL << 63);
	// The compiler will turn this ternary into a fast, branchless `cmov`.
	std::byte* base = is_global ? global_buffer : local_stack;
	return base + offset;
}

/**
 * @brief Writes a value of a given TYPE to a specified location on the stack or global buffer,
 * depending on the highest bit of the place argument.
 */
template<typename T>
[[nodiscard]] [[gnu::always_inline]]
inline static T readFromPlace(std::byte* local_stack, std::byte* global_buffer, u64 place_arg) {
	return vm::safeReadPointerBytes<T>(getBytePtrFromPlaceArg(local_stack, global_buffer, place_arg)
	);
}

/**
 * @brief Writes a value of a given TYPE to a specified location on the stack or global buffer,
 * depending on the highest bit of the place argument.
 */
template<typename T>
[[gnu::always_inline]]
inline static void writeToPlace(
	std::byte* local_stack, std::byte* global_buffer, u64 place_arg, const T& value
) {
	vm::safeWriteBytes<T>(getBytePtrFromPlaceArg(local_stack, global_buffer, place_arg), value);
}

/**
 * @brief Helper macros for reading/writing values from/to place arguments,
 * which can be either local or global depending on the highest bit of the argument.
 */
#define READ_FROM_PLACE_ARG(TYPE, ARG) \
	readFromPlace<TYPE>(local_stack, thread.runtime_data.global_data_buffer_base, ARG)
#define WRITE_TO_PLACE_ARG(TYPE, ARG, VALUE) \
	writeToPlace<TYPE>(local_stack, thread.runtime_data.global_data_buffer_base, ARG, VALUE)
// Resolves a block place argument, creating the local's block if it does not exist yet.
#define READ_BLOCK_REF_FROM_ARG(ARG) OpFuns::readBlockRefFromArg(frame, thread, ARG)
/**
 * @brief Helper macro for reading a value from a immediate argument.
 */
#define READ_FROM_DIRECT_ARG(TYPE, ARG) safeReadObjectBytes<TYPE>(ARG)

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
 * @brief Execute next instruction of the bytecode.
 * @param i indicates that the i-th next instruction will be executed,
 * with `0` being the current instruction.
 */
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
#define OPFUN_CONT(i)                                                                     \
	IF_TC({ MUST_TAIL return instr[i].tc_opfun(&instr[i], local_stack, frame, thread); }) \
	IF_NOT_TC({ instr += i; })
// NOLINTEND(cppcoreguidelines-pro-type-union-access)

// Prevent \ warning
