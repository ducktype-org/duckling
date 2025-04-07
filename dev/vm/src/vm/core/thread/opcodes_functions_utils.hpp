#pragma once

#include <base/ints.hpp>

#include <vm/core/process/memory/frame.hpp>
#include <vm/core/process/memory/memory.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>

template<typename T>
[[gnu::always_inline]]
inline static T& derefStack(std::byte* stack, i64 position) {
	return *(reinterpret_cast<T*>(&stack[position]));
}

[[gnu::always_inline]]
inline static vm::TypeCRef
	getLocalType(u64 offset, const vm::Frame* frame, const vm::Memory& memory) {
	auto block_idx = frame->local_offset_to_block_idx[offset];
	auto block     = frame->block_stack[block_idx];
	return memory.getBlockType(block);
}
