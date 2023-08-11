#pragma once

#include <base/ints.hpp>
#include <base/strongly_typed_int.hpp>

namespace vm {

	// @TODO: change to STRONG_TYPEDEF_IT when available
	STRONG_TYPEDEF_INT_DIMENSIONAL(BlockId, u64);

	class Pointer {
	private:
		BlockId block;
		u64 offset;
	public:
		Pointer(BlockId block_, u64 offset_): block(block_), offset(offset_) {}
		BlockId getBlock() const {
			return block;
		}
		u64 getOffset() const {
			return offset;
		}

		void addOffset(i64 off) {
			// @TODO: range checking
			offset += off;
		}
	};
}
