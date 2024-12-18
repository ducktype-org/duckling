#pragma once

#include <core/process/memory/allocator/allocator.hpp>
#include <core/process/memory/allocator/stack_allocator.hpp>
#include "memory_data/block.hpp"
#include <base/smart_pointers.hpp>

namespace vm {
	class VMProcess;

	/**
	 * @brief Holds metadata about all dynamic memory of the VCPU.
	 *
	 * This class is used to manage memory blocks of the VCPU.
	 * The memory used by the VCPU is divided into blocks. Each block
	 * has a unique ID, which is used to access the block, fixed size
	 * and a pointer to the data. Owner of the block is the one who
	 * creates it - mainly `Allocator` services or `Executor` itself.
	 *
	 * More information in the paper
	 * ["Prototyp maszyny
	 * wirtualnej..."](https://github.com/ducktype-org/dev-space/blob/main/prace_naukowe/maszyna_wirtualna.pdf)
	 */
	class Memory {
	private:
		Memory();

		friend class VMProcess;

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

		StackAllocator stack_allocator;
		Allocator      dynamic_allocator;

	public:
		using error = std::string;

		StackAllocator& getStackAllocator();
		Allocator&      getDynamicAllocator();

		BlockID reserveBlockID();
		void    returnBlockID(BlockID);

		[[gnu::always_inline]]
		inline Block* getBlock(BlockID id) {
			// @NOTE: disabling these checks increases
			// load/store performance in TC by eliminating
			// 4 stack push-pops in asm
			if (isUnowned(id))
				CORE_PANIC("Tried accessing unowned block");
			else if (!blocks[usize(id)].filled)
				CORE_PANIC("Tried accessing uninitialized block");
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
