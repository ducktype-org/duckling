#pragma once

#include <memory_data/block.hpp>
#include <base/smart_pointers.hpp>

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

		[[gnu::always_inline]]
		inline bool isUnowned(BlockId id) {
			return id >= high_id || !blocks[usize(id)].owned;
		}

	public:
		using error = std::string;

		BlockId reserveBlockID();
		void    returnBlockID(BlockId);

		[[gnu::always_inline]]
		inline Block* getBlock(BlockId id) {
			// @NOTE: disabling these checks increases
			// load/store performance in TC by eliminating
			// 4 stack push-pops in asm
			if (isUnowned(id))
				RIFT_PANIC("Tried accessing unowned block");
			else if (!blocks[usize(id)].filled)
				RIFT_PANIC("Tried accessing uninitialized block");
			return blocks[usize(id)].block;
		}

		void makeBlock(BlockId block_id, Block&& block);
		void deleteBlock(BlockId block_id);

		void createRef(BlockId block_id);
		void destroyRef(BlockId block_id);

		// @TODO: nullPtr deref errors, block ownership, etc
		[[nodiscard]]
		inline Pointer nullPtr() const {
			return Pointer{ null_block_id, 0 };
		}
	};
}
