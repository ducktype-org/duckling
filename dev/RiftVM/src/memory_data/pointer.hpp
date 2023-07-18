#pragma once

#include <cstdint>
#include <base/strongly_typed_int.hpp>

namespace vm {

	// @TODO: change to STRONG_TYPEDEF_IT when available
	STRONG_TYPEDEF_INT(BlockId, std::uint64_t);

	class Pointer {
	private:
		BlockId block;
		std::uint64_t offset;
	public:
		Pointer(BlockId block_, std::uint64_t offset_): block(block_), offset(offset_) {}
		BlockId getBlock() const {
			return block;
		}
		std::uint64_t getOffset() const {
			return offset;
		}

		void addOffset(std::int64_t off) {
			// @TODO: range checking
			offset += off;
		}
	};
}
