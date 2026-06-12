#pragma once

#include <base/misc/int_conv.hpp>
#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/exceptions.hpp>

namespace vm {

	template<typename EntryT>
	class BasicBlock;

	using Block = BasicBlock<std::byte>;

	template<typename EntryT, typename BlockT>
	class IMemory;

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

		constexpr Pointer(): block(nullptr), offset(0) {}

		template<typename E, typename B>
		friend class IMemory;

	public:
		constexpr Pointer(Ref<Block> block, u64 offset): block(block.get()), offset(offset) {}

		void movePointer(u64 move_by) {
			if (block == nullptr) throw exceptions::VMNullPointerAccessException();
			offset += move_by;
		}

		[[nodiscard]]
		Pointer movedPointer(u64 move_by) const {
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

		static constexpr Pointer null() { return {}; }

		constexpr bool operator==(const Pointer&) const = default;
	};

	static_assert(sizeof(Pointer) == 16);
}
