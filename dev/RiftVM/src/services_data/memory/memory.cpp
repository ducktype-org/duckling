#include "memory.hpp"

#include <base/exceptions.hpp>
#include <iostream>

namespace vm {
	bool Memory::isUnowned(BlockId id) {
		return id >= high_id || !blocks[usize(id)].owned;
	}

	BlockId Memory::reserveBlockID() {
		if (free_ids.empty()) {
			blocks.emplace_back();
			blocks.back().owned = true;
			RIFT_ASSERT(usize(high_id) == blocks.size() - 1, "Bad high id");
			return high_id++;
		}
		BlockId res = free_ids.back();
		free_ids.pop_back();
		return res;
	}

	bool Memory::refCheck(BlockId id) {
		if (blocks[usize(id)].refcount == 0) {
			if (id == high_id - BlockId(1)) {
				high_id--;
				blocks.pop_back();
			} else {
				free_ids.push_back(id);
			}
			return false;
		}
		return true;
	}

	void Memory::returnBlockID(BlockId id) {
		if (isUnowned(id)) {
			RIFT_PANIC("Tried returning an unowned id");
		} else if (blocks[usize(id)].filled) {
			RIFT_PANIC("Tried returning an id of an unfreed block");
		}
		refCheck(id);
	}

	cpp::result<Block*, Memory::error> Memory::getBlock(BlockId id) {
		if (isUnowned(id)) {
			return cpp::fail("Tried accessing unowned block");
		} else if (!blocks[usize(id)].filled) {
			return cpp::fail("Tried accessing uninitialized block");
		}
		return blocks[usize(id)].block;
	}

	void Memory::makeBlock(BlockId id, Block&& block) {
		if (isUnowned(id)) {
			RIFT_PANIC("Tried creating an unowned block");
		} else if (blocks[usize(id)].filled) {
			RIFT_PANIC("Tried creating an initialized block");
		}
		blocks[usize(id)].block  = new Block(std::move(block));

		// @TODO: this assumes every block is initialized
		blocks[usize(id)].filled = true;
	}

	void Memory::deleteBlock(BlockId id) {
		if (isUnowned(id)) {
			RIFT_PANIC("Tried deleting an unowned block");
		}
		blocks[usize(id)].filled = false;
		delete blocks[usize(id)].block;

		// @FIXME: refCheck deleted BlockData if ref count is zero
		// Issue: https://github.com/rift-lang/rift-poc-zpp1/issues/90
		// this can cause memory error here:
		// this code only make sense if refCheck didn't delete block

		// if (!refCheck(id)) {
		// 	blocks[usize(id)].deleted = false;
		// } else {
		// 	blocks[usize(id)].deleted = true;
		// }
		blocks[usize(id)].deleted = false;
	}

	void Memory::createRef(BlockId id) {
		if (isUnowned(id)) {
			RIFT_PANIC("Tried creating a reference to an unowned block");
		} else if (!blocks[usize(id)].filled) {
			RIFT_PANIC("Tried creating a reference to an uninitialized block");
		}
		blocks[usize(id)].refcount++;
	}

	void Memory::destroyRef(BlockId id) {
		if (id >= high_id || (!blocks[usize(id)].owned && !blocks[usize(id)].deleted)) {
			RIFT_PANIC("Tried deleting a reference to an unowned block");
		}
		if (blocks[usize(id)].refcount == 0) {
			RIFT_PANIC("Tried deleting a reference to an unreferenced block");
		}
		if (--blocks[usize(id)].refcount == 0 && blocks[usize(id)].deleted) {
			blocks[usize(id)].deleted = true;
			refCheck(id);
		}
	}

	Pointer Memory::nullPtr() const {
		return Pointer{ null_block_id, 0 };
	}
}
