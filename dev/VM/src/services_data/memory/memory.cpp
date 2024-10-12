#include <iostream>
#include "memory.hpp"
#include <base/exceptions.hpp>

namespace vm {
	BlockID Memory::reserveBlockID() {
		if (free_ids.empty()) {
			blocks.emplace_back();
			blocks.back().owned = true;
			CORE_ASSERT(usize(high_id) == blocks.size() - 1, "Bad high id");
			return high_id++;
		}
		BlockID res = free_ids.back();
		free_ids.pop_back();
		return res;
	}

	bool Memory::refCheck(BlockID id) {
		if (blocks[usize(id)].refcount == 0) {
			if (id == high_id - BlockID(1)) {
				high_id--;
				blocks.pop_back();
			} else {
				free_ids.push_back(id);
			}
			return false;
		}
		return true;
	}

	void Memory::returnBlockID(BlockID id) {
		if (isUnowned(id))
			CORE_PANIC("Tried returning an unowned id");
		else if (blocks[usize(id)].filled)
			CORE_PANIC("Tried returning an id of an unfreed block");
		refCheck(id);
	}

	void Memory::makeBlock(BlockID id, Block&& block) {
		if (isUnowned(id))
			CORE_PANIC("Tried creating an unowned block");
		else if (blocks[usize(id)].filled)
			CORE_PANIC("Tried creating an initialized block");
		blocks[usize(id)].block = new Block(std::move(block));

		// @TODO: this assumes every block is initialized
		blocks[usize(id)].filled = true;
	}

	void Memory::deleteBlock(BlockID id) {
		if (isUnowned(id)) CORE_PANIC("Tried deleting an unowned block");
		blocks[usize(id)].filled = false;
		delete blocks[usize(id)].block;

		// @FIXME: refCheck deleted BlockData if ref count is zero
		// Issue: https://github.com/ducktype-org/rift-poc-zpp1/issues/90
		// this can cause memory error here:
		// this code only make sense if refCheck didn't delete block

		// if (!refCheck(id)) {
		// 	blocks[usize(id)].deleted = false;
		// } else {
		// 	blocks[usize(id)].deleted = true;
		// }
		blocks[usize(id)].deleted = false;
	}

	void Memory::createRef(BlockID id) {
		if (isUnowned(id))
			CORE_PANIC("Tried creating a reference to an unowned block");
		else if (!blocks[usize(id)].filled)
			CORE_PANIC("Tried creating a reference to an uninitialized block");
		blocks[usize(id)].refcount++;
	}

	void Memory::destroyRef(BlockID id) {
		if (id >= high_id || (!blocks[usize(id)].owned && !blocks[usize(id)].deleted))
			CORE_PANIC("Tried deleting a reference to an unowned block");
		if (blocks[usize(id)].refcount == 0)
			CORE_PANIC("Tried deleting a reference to an unreferenced block");
		if (--blocks[usize(id)].refcount == 0 && blocks[usize(id)].deleted) {
			blocks[usize(id)].deleted = true;
			refCheck(id);
		}
	}
}
