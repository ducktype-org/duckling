#include "ctv.hpp"

namespace exec {

	std::vector<Block>& getBlocks() {
		// @TODO: static order initialization fiasco [solve it better]
		static std::vector<Block> blocks;
		return blocks;
	}

	CTV alloc_new(ts::TypeDesc<> type, usize size) {
		BlockId block = Block::create(size);
		Pointer pointer{block, 0};
		CTV     ctv(type, pointer, size);
		return ctv;
	}

}
