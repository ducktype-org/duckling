#pragma once

#include <base/memory/manual_lifetime_storage.hpp>
#include <base/misc/noexcept.hpp>
#include <base/pointers/box.hpp>

namespace base {

	/**
	 * A memory pool object allocator of a single type.
	 * It manages objects, not memory.
	 * On destruction frees the allocated memory, panics (in dev-builds) if not all allocated
	 * objects have been deallocated.
	 *
	 * It is not a valid c++-allocator.
	 *
	 * @note After some IRL debates it was concluded that allocate/deallocate api should
	 * work on plain Refs, and deallocation must internally search for a buffer and an index of the
	 * object. This might be less time efficient, but it greatly simplifies the api and lowers
	 * memory usage and can be made in logarithmic time if needed in the future.
	 */
	template<class T, u64 BYTE_BLOCK_SIZE = 4 * 1'024>
	requires base::IsPlainType<T> class SingleTypeMemoryPoolAllocator final {
	private:
		using StorageT = ManualLifetimeStorage<T>;

		constexpr static u64 BLOCK_ELEMENT_COUNT = BYTE_BLOCK_SIZE / sizeof(StorageT);
		using ItemArray                          = std::array<StorageT, BLOCK_ELEMENT_COUNT>;

		static_assert(
			sizeof(ItemArray) == BLOCK_ELEMENT_COUNT * sizeof(StorageT), "ItemArray size mismatch"
		);

		static_assert(BLOCK_ELEMENT_COUNT > 0, "BYTE_BLOCK_SIZE is too small for type T");

		constexpr static u64 PADDING_BYTES_COUNT = BYTE_BLOCK_SIZE - sizeof(ItemArray);

		struct BufferNoPadding final {
			ItemArray items;
		};

		template<u64 N>
		struct BufferWithPadding final {
			ItemArray items;

			[[maybe_unused]]
			char padding[N];  // NOLINT
		};

		/**
		 * A single buffer (i.e. "pool") of objects.
		 */
		using Buffer = std::conditional_t<
			PADDING_BYTES_COUNT == 0,
			BufferNoPadding,
			BufferWithPadding<PADDING_BYTES_COUNT>>;

		static_assert(
			sizeof(Buffer) == BYTE_BLOCK_SIZE, "Buffer size must be equal to BYTE_BLOCK_SIZE"
		);

		/**
		 * Simple helper type that wraps index of a buffer and index of an item within that buffer.
		 */
		struct BufferItemIndex final {
			u64 buffer_idx = 0;
			u64 item_idx   = 0;

			BufferItemIndex next() const {
				BufferItemIndex next_idx = *this;
				next_idx.item_idx++;
				if (next_idx.item_idx >= BLOCK_ELEMENT_COUNT) [[unlikely]] {
					next_idx.buffer_idx++;
					next_idx.item_idx = 0;
				}
				return next_idx;
			}
		};

	public:
		SingleTypeMemoryPoolAllocator()                                     = default;
		SingleTypeMemoryPoolAllocator(const SingleTypeMemoryPoolAllocator&) = delete;

		SingleTypeMemoryPoolAllocator(SingleTypeMemoryPoolAllocator&& other) noexcept:
			  buffers(std::move(other.buffers)),
			  free_list(std::move(other.free_list)),
			  IF_BUILD_TYPE_DEV(allocated_count(other.allocated_count) COMMA)
				  next_item_idx(other.next_item_idx) {
			IF_BUILD_TYPE_DEV(other.allocated_count = 0;)
			other.next_item_idx = BufferItemIndex{};
		}

		SingleTypeMemoryPoolAllocator& operator=(const SingleTypeMemoryPoolAllocator&) = delete;
		SingleTypeMemoryPoolAllocator& operator=(SingleTypeMemoryPoolAllocator&&)      = delete;

		/**
		 * Allocates a new object in the pool and constructs it with the given arguments.
		 */
		Ref<T> allocateEmplace(auto&&... args) {
			IF_BUILD_TYPE_DEV(allocated_count++;);

			// idx, in which we will allocate the new object:
			BufferItemIndex allocation_idx;

			// this will be unlikely in non-lsp scenarios:
			if (not free_list.empty()) [[unlikely]] {
				allocation_idx = free_list.back();
				free_list.pop_back();
			} else {
				// else allocate in the next available slot:
				allocation_idx = next_item_idx;
				next_item_idx  = next_item_idx.next();

				// check if we need to allocate a new buffer:
				if (allocation_idx.buffer_idx >= buffers.size()) [[unlikely]]
					buffers.emplace_back(makeBox<Buffer>());
			}

			CORE_ASSERT(allocation_idx.buffer_idx < buffers.size(), "Buffer index out of bounds");
			CORE_ASSERT(allocation_idx.item_idx < BLOCK_ELEMENT_COUNT, "Item index out of bounds");

			Ref<StorageT> new_storage
				= &buffers[allocation_idx.buffer_idx]->items[allocation_idx.item_idx];
			new_storage->construct(std::forward<decltype(args)>(args)...);
			return new_storage->get();
		}

		/**
		 * Destroys the given object without deallocating its memory.
		 * The allocator will treat the memory as still allocated.
		 * It will be freed when the allocator is destroyed.
		 *
		 * @note If object pointed to by obj_ref was not allocated by this allocator,
		 * behavior is undefined, EVEN IN DEV BUILDS.
		 *
		 * @note This could be made static in release builds,
		 * but we don't do it for simplicity.
		 * It can be made static later if needed.
		 */
		void justDestroy(Ref<T> obj_ref) {
			IF_BUILD_TYPE_DEV(allocated_count--;)
			Ref<StorageT> obj_storage = StorageT::getSelf(obj_ref);
			obj_storage->destroy();
		}

		/**
		 * Deallocates and destroys the given object.
		 * Frees the memory for future allocations.
		 *
		 * @note If object pointed to by obj_ref was not allocated by this allocator,
		 * behavior is undefined, EVEN IN DEV BUILDS.
		 */
		void deallocateDestroy(Ref<T> obj_ref) {
			IF_BUILD_TYPE_DEV(allocated_count--;)

			Ref<StorageT> obj_storage = StorageT::getSelf(obj_ref);

			// idx, in which the object is stored:
			BufferItemIndex deallocation_idx;

			bool found = false;

			// naively find the buffer and the index within the buffer:
			for (u64 buffer_idx = 0; buffer_idx < buffers.size(); buffer_idx++) {
				StorageT* buffer_pointer_start = std::begin(buffers[buffer_idx]->items);
				StorageT* buffer_pointer_end   = std::end(buffers[buffer_idx]->items);
				if (std::less_equal<>{}(buffer_pointer_start, obj_storage.get())
				    and std::less<>{}(obj_storage.get(), buffer_pointer_end)) {
					deallocation_idx.buffer_idx = buffer_idx;
					deallocation_idx.item_idx
						= static_cast<u64>(obj_storage.get() - buffer_pointer_start);
					found = true;

					CORE_ASSERT(
						obj_ref.get() == buffers[buffer_idx]->items[deallocation_idx.item_idx].get(),
						"Calculated deallocation index does not point to the given object"
					);

					break;
				}
			}

			CORE_ASSERT(found, "Object to deallocate was not allocated by this allocator");

			// actually destroy and deallocate the object:
			obj_storage->destroy();
			free_list.push_back(deallocation_idx);
		}

		IF_BUILD_TYPE_DEV(~SingleTypeMemoryPoolAllocator() {
			CORE_ASSERT_NOEXCEPT(
				allocated_count == 0,
				"Not all allocated objects were deallocated before destruction of the allocator. ",
				"Leaked objects count: ",
				allocated_count
			);
		})
		IF_BUILD_TYPE_RELEASE(~SingleTypeMemoryPoolAllocator() = default;)

	private:
		/**
		 * Actual storage for all allocated buffers.
		 * They are boxed, so that they are not moved in memory when the vector resizes.
		 */
		std::vector<Box<Buffer>> buffers;

		/**
		 * List of free items in the pools.
		 */
		std::vector<BufferItemIndex> free_list;

		/**
		 * Total number of allocated items in the pool.
		 * It is used only in dev builds to assert correct usage.
		 */
		IF_BUILD_TYPE_DEV(u64 allocated_count = 0;)

		/**
		 * Next (buffer,item) index to allocate in, when free list is empty.
		 */
		BufferItemIndex next_item_idx = { 0, 0 };
	};
}
