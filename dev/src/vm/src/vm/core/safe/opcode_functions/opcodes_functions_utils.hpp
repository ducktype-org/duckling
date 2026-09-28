#pragma once

#include "../../config.hpp"

#include <base/misc/int_conv.hpp>
#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/musttail.hpp>
#include <vm/core/process/concurrency/fast_track/shadow_entry.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/memory/frame.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>
#include <vm/core/safe/safe_vmthread.hpp>
#include <vm/utils/interpret.hpp>

namespace vm {
	template<typename EntryT>
	class GenericBlock;
	using Block = GenericBlock<byte>;
}

/**
 * @brief Given the argument of type "place", which is an offset to the global buffer or local
 * stack, with the highest bit indicating whether it's global or local, returns the pointer to the
 * actual byte in memory.
 * @param arg - the mentioned argument
 */
[[gnu::always_inline]]
inline static byte* getBytePtrFromPlaceArg(byte* local_stack, byte* global_buffer, u64 arg) {
	// Extract the highest bit.
	bool is_global = (arg >> 63) != 0;

	// Mask out the highest bit to get the offset.
	u64 offset = arg & ~(1ULL << 63);
	// The compiler will turn this ternary into a fast, branchless `cmov`.
	byte* base = is_global ? global_buffer : local_stack;
	return base + offset;
}

/**
 * @brief Writes a value of a given TYPE to a specified location on the stack or global buffer,
 * depending on the highest bit of the place argument.
 */
template<typename T>
[[nodiscard]] [[gnu::always_inline]]
inline static T readFromPlace(byte* local_stack, byte* global_buffer, u64 place_arg) {
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
	byte* local_stack, byte* global_buffer, u64 place_arg, const T& value
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
 * @brief Convenience macros for accessing the Fast Track state from `ft_*` opcode functions.
 * The `ft_*` opcodes are only emitted for a process with `enable_fast_track`, so the state is
 * always present when they execute.
 */
#define FT_DATA          (*thread.ft_data)
#define FT_RT            (FT_DATA.ft_runtime)
#define FT_GLOBALS       (thread.safe_process.getFastTrackGlobals())
#define FT_SHADOW_MEMORY (FT_GLOBALS.getShadowDataMemory())
// Expands to the three args every processRead/processWrite call needs.
#define FT_EPOCH_ARGS FT_DATA.thread_id, FT_DATA.getVC()[FT_DATA.thread_id], FT_DATA.getVC()

/**
 * @brief The highest bit of a shadow place argument marks a global; the rest is the offset in
 * the global or the frame's local shadow data.
 */
[[nodiscard]] [[gnu::always_inline]]
inline static bool isGlobalPlace(u64 place_arg) {
	return (place_arg >> 63) != 0;
}

[[nodiscard]] [[gnu::always_inline]]
inline static vm::ShadowEntry* getShadowEntryPtr(
	const vm::FastTrackThreadData& ft_data, u64 place_arg
) {
	const u64 offset = place_arg & ~(1ULL << 63);
	if (isGlobalPlace(place_arg)) return ft_data.getGlobalShadowDataBase() + offset;
	return ft_data.getShadowFrame()->local_shadow_data_stack + offset;
}

#define GET_SHADOW_ENTRY_PTR(ARG) getShadowEntryPtr(FT_DATA, ARG)

/**
 * @brief Pushes the shadow frame of a call to `called_func`, the shadow counterpart of
 * `performFunctionCall`. The callee's shadow data starts at the caller's return and argument
 * entries: the caller lends it both, and `ft_ret` hands the return entries back, so a call
 * leaves the caller's shadow head exactly where the data side leaves its slot stack.
 */
[[gnu::always_inline]]
inline static void pushShadowFrame(
	vm::FastTrackRuntimeData& ft_runtime, const vm::low::LowFuncData& called_func
) {
	const u64 shared_shadow_data_size = called_func.arg_shadow_size + called_func.ret_shadow_size;

	auto* prev_sf = ft_runtime.shadow_frame_stack_current;
	auto* sf      = prev_sf + 1;
	if (sf + 1 >= ft_runtime.shadow_frame_stack_end)
		throw vm::exceptions::VMStackOverflowException();

	sf->local_shadow_data_stack = prev_sf->local_shadow_data_stack
	                            + (prev_sf->local_shadow_data_head - shared_shadow_data_size);
	sf->local_shadow_data_head = base::safeIntConv<u32>(shared_shadow_data_size);
	prev_sf->local_shadow_data_head -= base::safeIntConv<u32>(shared_shadow_data_size);

	ft_runtime.shadow_frame_stack_current = sf;
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
	vm::safeWriteBytes<T>(view.getBegin(), value);
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
