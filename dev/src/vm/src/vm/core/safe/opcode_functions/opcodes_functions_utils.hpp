#pragma once

#include "../../config.hpp"

#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/memory/block.hpp>
#include <vm/core/safe/memory/frame.hpp>
#include <vm/utils/interpret.hpp>
#include <vm/core/safe/safe_vmthread.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>
#include <vm/core/safe/fast_track_safe_vmthread.hpp>
#include <vm/core/safe/fast_track_safe_vmprocess.hpp>
#include <vm/core/safe/low_program/low_program.hpp>

namespace vm {
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
 * @brief Given the argument of type "place", which is an index in the local block reference stack
 * or global block reference buffer, with the highest bit indicating whether it's global or local,
 * returns the actual block reference.
 * @param arg - the mentioned argument
 */
[[nodiscard]] [[gnu::always_inline]]
inline static Ref<vm::Block> getBlockRefFromArg(
	vm::Block** local_stack_blocks, vm::Block** global_buffer_blocks, u64 arg
) {
	// Extract the highest bit.
	bool is_global = (arg >> 63) != 0;

	// Mask out the highest bit to get the offset.
	u64 offset = arg & ~(1ULL << 63);

	vm::Block** base = is_global ? global_buffer_blocks : local_stack_blocks;
	return { base[offset] };
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
#define READ_BLOCK_REF_FROM_ARG(ARG)                                                             \
	getBlockRefFromArg(                                                                          \
		frame->local_block_ref_stack_base, thread.runtime_data.global_block_ref_buffer_base, ARG \
	)
#define READ_FROM_DIRECT_ARG(TYPE, ARG) safeReadObjectBytes<TYPE>(ARG)

[[nodiscard]] [[gnu::always_inline]]
inline static Ref<vm::ShadowBlock> getShadowBlockRefFromArg(
	vm::FastTrackSafeVMThread& ft_thread, vm::ShadowBlock** local_stack_blocks, u64 arg
) {
	bool is_global = (arg >> 63) != 0;
	u64  offset    = arg & ~(1ULL << 63);
	if (is_global) {
		return static_cast<vm::FastTrackSafeVMProcess&>(ft_thread.getProcess())
		    .getFTGlobals()
		    .getGlobalShadowBlock(offset);
	} else {
		return { local_stack_blocks[offset] };
	}
}

[[nodiscard]] [[gnu::always_inline]]
inline static Ref<vm::ShadowPointerBlock> getShadowPointerBlockRefFromArg(
	vm::FastTrackSafeVMThread& ft_thread, vm::ShadowPointerBlock** local_stack_blocks, u64 arg
) {
	bool is_global = (arg >> 63) != 0;
	u64 offset = arg & ~(1ULL << 63);
	if (is_global) {
		return static_cast<vm::FastTrackSafeVMProcess&>(ft_thread.getProcess())
		    .getFTGlobals()
		    .getGlobalShadowPointerBlock(offset);
	} else {
		return { local_stack_blocks[offset] };
	}
}

/**
 * @brief Convenience macros for accessing FastTrack data from ft_* opcode functions.
 * The thread argument is always SafeVMThread& but ft_* opcodes only execute when a
 * FastTrackSafeVMThread is in use, so the downcast is safe.
 */
#define FT_THREAD (static_cast<vm::FastTrackSafeVMThread&>(thread))
#define FT_DATA   (FT_THREAD.ft_data)
#define FT_RT     (FT_DATA.ft_runtime)

#define READ_SHADOW_BLOCK_REF_FROM_ARG(ARG) \
	getShadowBlockRefFromArg( \
		FT_THREAD, FT_RT.shadow_block_ref_stack_base, ARG \
	)

#define READ_SHADOW_POINTER_BLOCK_REF_FROM_ARG(ARG) \
	getShadowPointerBlockRefFromArg( \
		FT_THREAD, FT_RT.shadow_pointer_block_ref_stack_base, ARG \
	)

inline static vm::ShadowPointer updateShadowPointerAssignment(
	vm::FastTrackSafeVMThread& ft_thread, vm::ShadowPointer dst, vm::ShadowPointer src
) {
	if (dst.shadow_block != src.shadow_block) {
		if (dst.shadow_block) {
			ft_thread.getShadowDataMemory().decreaseBlockRefcount(dst.shadow_block.toOpt().value());
		}
		if (src.shadow_block) {
			ft_thread.getShadowDataMemory().increaseBlockRefcount(src.shadow_block.toOpt().value());
		}
	}
	if (dst.shadow_pointer_block != src.shadow_pointer_block) {
		if (dst.shadow_pointer_block) {
			ft_thread.getShadowPointerMemory().decreaseBlockRefcount(dst.shadow_pointer_block.toOpt().value());
		}
		if (src.shadow_pointer_block) {
			ft_thread.getShadowPointerMemory().increaseBlockRefcount(src.shadow_pointer_block.toOpt().value());
		}
	}
	return src;
}


[[nodiscard]] [[gnu::always_inline]]
inline static bool isGlobalPlace(u64 place_arg) {
	return (place_arg >> 63) != 0;
}

[[nodiscard]] [[gnu::always_inline]]
inline static vm::ShadowPointer& getShadowPointerRef(
	vm::Frame*, vm::FastTrackSafeVMThread& ft_thread, u64 place_arg
) {
	u64 offset = place_arg & ~(1ULL << 63);
	if (isGlobalPlace(place_arg)) {
		return ft_thread.getFTData().getGlobalShadowPointerBase()[offset];
	} else {
		return ft_thread.getFTData().getShadowFrame()->local_shadow_pointer_stack[offset];
	}
}

[[nodiscard]] [[gnu::always_inline]]
inline static vm::ShadowEntry* getShadowEntryPtr(
	vm::Frame*, vm::FastTrackSafeVMThread& ft_thread, u64 place_arg
) {
	u64 offset = place_arg & ~(1ULL << 63);
	if (isGlobalPlace(place_arg)) {
		return ft_thread.getFTData().getGlobalShadowDataBase() + offset;
	} else {
		return ft_thread.getFTData().getShadowFrame()->local_shadow_data_stack + offset;
	}
}

#define GET_SHADOW_POINTER_REF(ARG) getShadowPointerRef(frame, FT_THREAD, ARG)
#define GET_SHADOW_ENTRY_PTR(ARG)   getShadowEntryPtr(frame, FT_THREAD, ARG)

/**
 * @brief Reads a value of a given TYPE from the beginning of the given view.
 */
template<typename T, typename EntryT>
[[nodiscard]] [[gnu::always_inline]]
inline static T readFromView(base::TypedModRawView<EntryT> view) {
	return vm::safeReadPointerBytes<T>(view.getBegin());
}

/**
 * @brief Writes a value of a given TYPE to the beginning of the given view.
 */
template<typename T, typename EntryT>
[[gnu::always_inline]]
inline static void writeToView(base::TypedModRawView<EntryT> view, const T& value) {
	vm::safeWriteBytes<T>(view.getBegin(), value);
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

// Prevent \ warning
