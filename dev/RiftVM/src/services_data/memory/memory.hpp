#pragma once

#include <base/smart_pointers.hpp>
#include <memory_data/block.hpp>

namespace vm {
	template<class... DynamicData>
	class DataManagerDef;

	class Memory {
	private:
		Memory() = default;

		template<class... DynamicData>
		friend class DataManagerDef;

		struct BlockData {
			u64    refcount = 0;
			bool   owned    = false;
			bool   filled   = false;
			bool   deleted  = false;
			Block* block    = nullptr;
		};

		static constexpr usize   special_blocks_count = 1;
		static constexpr BlockId null_block_id        = BlockId(-1);

		std::vector<BlockData> blocks = {};

		// high_id is lowest non-owned id
		BlockId high_id = BlockId(0);

		std::vector<BlockId> free_ids = {};

		bool refCheck(BlockId block_id);

		bool isUnowned(BlockId block_id);

	public:
		using error = std::string;

		BlockId reserveBlockID();
		void    returnBlockID(BlockId);

		cpp::result<Block*, error> getBlock(BlockId block_id);

		void makeBlock(BlockId block_id, Block&& block);
		void deleteBlock(BlockId block_id);

		void createRef(BlockId block_id);
		void destroyRef(BlockId block_id);

		// @TODO: nullPtr deref errors, block ownership, etc
		Pointer nullPtr() const;
	};
}
