#pragma once

#include <base/misc/int_conv.hpp>
#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/exceptions.hpp>

namespace vm {

	template<typename EntryT>
	class BasicBlock;

	template<typename EntryT, typename BlockT>
	class IMemory;

	struct ShadowEntry;

	/**
	 * @brief Basic pointer used in the VM.
	 * Contains the pointer to the block and the offset in the block.
	 *
	 * Most of its methods are moved to the static methods of the Memory class,
	 * to avoid circular dependencies and because it requires nontrivial logic.
	 * It's good to think that the Memory class governs the pointers.
	 */
	template<typename EntryT>
	class BasicPointer final {
	private:
		MRef<BasicBlock<EntryT>> block;
		u64                      offset;

		friend class IMemory<EntryT, BasicBlock<EntryT>>;

	public:
		BasicPointer(): block(nullptr), offset(0) {}

		BasicPointer(Ref<BasicBlock<EntryT>> block, u64 offset): block(block.get()), offset(offset) {}

		void movePointer(u64 move_by) {
			if (block == nullptr) throw exceptions::VMNullPointerAccessException();
			offset += move_by;
		}

		[[nodiscard]]
		BasicPointer movedPointer(u64 move_by) const {
			BasicPointer cpy(*this);
			cpy.movePointer(move_by);
			return cpy;
		}

		[[nodiscard]]
		auto getBlock() const -> Ref<BasicBlock<EntryT>> {
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

		static BasicPointer null() { return {}; }

		constexpr bool operator==(const BasicPointer&) const = default;
	};

	using Pointer = BasicPointer<std::byte>;

	using ShadowPointer = BasicPointer<ShadowEntry>;

	static_assert(sizeof(Pointer) == 16);
}
