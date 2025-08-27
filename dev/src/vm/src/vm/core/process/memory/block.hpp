#pragma once

#include <base/ints.hpp>
#include <base/maps.hpp>
#include <base/raw_view.hpp>
#include <base/strongly_typed_id.hpp>

#include <vm/core/process/memory/allocator/block_data.hpp>

#include <mutex>
#include <shared_mutex>
#include <utility>

namespace vm {

	class BlockID final {
	private:
		constexpr static u64 BAD_ID = u64(-1);
		u64                  id     = BAD_ID;

	public:
		inline constexpr explicit BlockID(u64 id): id{ id } {}

		constexpr BlockID()                                  = default;
		inline constexpr BlockID(const BlockID& mX)                 = default;
		inline constexpr BlockID(BlockID&& mX) noexcept             = default;
		inline constexpr BlockID& operator=(const BlockID& rhs)     = default;
		inline constexpr BlockID& operator=(BlockID&& rhs) noexcept = default;

		static BlockID bad() { return BlockID{ BAD_ID }; }

		static BlockID fromU64(u64 v) { return BlockID{ v }; }

		[[nodiscard]] inline constexpr explicit operator u64() const noexcept { return id; }

		[[nodiscard]] inline constexpr u64 asInt() const noexcept { return id; }

		auto operator<=>(const BlockID&) const = default;

		inline bool isBad() const { return id == BAD_ID; }

		inline bool isGood() const { return id != BAD_ID; }
	};

	;

	/**
	 * @brief Main block data structure.
	 *
	 * Holds all the block metadata and pointers to the real data.
	 * The blocks are managed by the `vm::Memory` class.
	 */
	class Block {
		/**
		 * @brief The unique identifier for the block.
		 */
		BlockID id;

		/**
		 * @brief The data of the block.
		 */
		BlockData data;

		/**
		 * @brief Flag whether the block has been deallocated.
		 */
		bool deallocated = false;

		/**
		 * @brief Flag whether the block is used.
		 * The block is not used, if it is inside the "free_ids" list of the Memory class.
		 * It can be reused for a new block.
		 */
		bool used = true;

		/**
		 * @brief The reference count of the block.
		 */
		u64 refcount = 0;

		/**
		 * @brief Pointer to the mutex.
		 * To avoid double dereference through the Memory class object.
		 */
		Ref<std::recursive_mutex> mutex_ref;

		// For future:
		// allocated at ...
		// freed at ...
		// name ...

		friend class Memory;

		// Think of it as a view on parent's bytes that has it's own type and lifetime.
		base::Map<usize, Ref<Block>> children_blocks{};  // offset to block
		MRef<Block>                  parent = nullptr;

	public:
		Block(BlockID id, BlockData data, Ref<std::recursive_mutex> mutex):
			  id(id),
			  data(data),
			  mutex_ref(mutex) {}
	};
}
