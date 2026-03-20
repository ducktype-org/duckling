#pragma once

#include <base/misc/int_conv.hpp>
#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/process/exceptions.hpp>

namespace vm {

	class Block;

	/**
	 * @brief Basic pointer used in the VM.
	 * Contains the pointer to the block and the offset in the block.
	 *
	 * Most of its methods are moved to the static methods of the Memory class,
	 * to avoid circular dependencies and because it requires nontrivial logic.
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
			if (block == nullptr) throw exceptions::VMNullPointerAccessException();
			if (move_by < 0 && base::safeIntConv<u64>(-move_by) > offset)
				throw exceptions::VMNegativeOffsetException();
			offset = base::safeIntConv<u64>(base::safeIntConv<i64>(offset) + move_by);
		}

		[[nodiscard]]
		Pointer movedPointer(i64 move_by) const {
			Pointer cpy(*this);
			cpy.movePointer(move_by);
			return cpy;
		}

		[[nodiscard]]
		auto getBlock() const -> Ref<Block> {
			if (block == nullptr) throw exceptions::VMNullPointerAccessException();
			return &*block;
		}

		[[nodiscard]]
		auto getOffset() const {
			return offset;
		}

		[[nodiscard]]
		auto isNull() const -> bool {
			return block == nullptr;
		}

		operator bool() const { return !isNull(); }

		static Pointer null() { return {}; }

		constexpr bool operator==(const Pointer&) const = default;
	};

	static_assert(sizeof(Pointer) == 16);
}
