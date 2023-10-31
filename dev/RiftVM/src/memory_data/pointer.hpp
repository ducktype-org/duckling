#pragma once

#include <base/ints.hpp>
#include <base/strongly_typed_int.hpp>

namespace vm {

	// @TODO: change to STRONG_TYPEDEF_IT when available
	STRONG_TYPEDEF_INT_DIMENSIONAL(BlockId, u64);

	class Pointer {
	private:
		BlockId block;
		u64     offset;

	public:
		Pointer() = default;

		Pointer(BlockId block_, u64 offset_): block(block_), offset(offset_) {}

		Pointer(Pointer&& other) noexcept: block(other.block), offset(other.offset) {}

		Pointer(Pointer& other) noexcept = default;

		BlockId getBlock() const { return block; }

		u64 getOffset() const { return offset; }

		void addOffset(i64 off) {
			// @TODO: range checking
			offset += off;
		}
	};
}
