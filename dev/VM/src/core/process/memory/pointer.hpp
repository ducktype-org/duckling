#pragma once

#include <base/ints.hpp>
#include <base/ref.hpp>
#include <base/raw_view.hpp>

namespace vm {

	class Block;

	/**
	 * @brief Basic pointer used in the VM.
	 * Contains the pointer to the block and the offset in the block.
	 *
	 * Most of it's methods are moved to the static methods of the Memory class,
	 * to avoid circular dependencies and becasue it requires nontrivial logic.
	 * It's good to think that the Memory class governs the pointers.
	 */
	class Pointer final {
	private:
		MRef<Block> block;
		u64         offset;

		Pointer(): block(nullptr), offset(0) {}

		friend class Memory;

	public:
		Pointer(Ref<Block> block, u64 offset): block(block.get()), offset(offset) {}

		void movePointer(i64 move_by) {
			if (block == nullptr) CORE_PANIC("Accessing null pointer");
			offset += move_by;
		}

		[[nodiscard]]
		auto getBlock() -> Ref<Block> {
			if (block == nullptr) CORE_PANIC("Accessing null pointer");
			return block.toOpt()->get();
		}

		static Pointer null() { return {}; }
	};
}
