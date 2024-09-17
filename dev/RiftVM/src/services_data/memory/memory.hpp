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
		static constexpr BlockID null_block_id        = BlockID(-1);

		std::vector<BlockData> blocks = {};

		// high_id is lowest non-owned id
		BlockID high_id = BlockID(0);

		std::vector<BlockID> free_ids = {};

		bool refCheck(BlockID block_id);

		[[gnu::always_inline]]
		inline bool isUnowned(BlockID id) {
			return id >= high_id || !blocks[usize(id)].owned;
		}

	public:
		using error = std::string;

		BlockID reserveBlockID();
		void    returnBlockID(BlockID);

		[[gnu::always_inline]]
		inline Block* getBlock(BlockID id) {
			// @NOTE: disabling these checks increases
			// load/store performance in TC by eliminating
			// 4 stack push-pops in asm
			if (isUnowned(id))
				RIFT_PANIC("Tried accessing unowned block");
			else if (!blocks[usize(id)].filled)
				RIFT_PANIC("Tried accessing uninitialized block");
			return blocks[usize(id)].block;
		}

		void makeBlock(BlockID block_id, Block&& block);
		void deleteBlock(BlockID block_id);

		void createRef(BlockID block_id);
		void destroyRef(BlockID block_id);

		// @TODO: nullPtr deref errors, block ownership, etc
		[[nodiscard]]
		inline Pointer nullPtr() const {
			return Pointer{ null_block_id, 0 };
		}
	};
}
