#pragma once

#include <base/ints.hpp>
#include <base/strongly_typed_int.hpp>

namespace vm {

	// @TODO: change to STRONG_TYPEDEF_IT when available
	STRONG_TYPEDEF_INT_DIMENSIONAL(BlockID, u64);

	class Pointer {
	private:
		BlockID block;
		u64     offset;

	public:
		Pointer(BlockID block_, u64 offset_): block(block_), offset(offset_) {}

		[[nodiscard]]
		inline BlockID getBlock() const {
			return block;
		}

		[[nodiscard]]
		inline u64 getOffset() const {
			return offset;
		}

		void addOffset(i64 off) {
			// @TODO: range checking
			offset += off;
		}
	};
}
