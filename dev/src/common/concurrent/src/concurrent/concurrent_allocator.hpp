#pragma once

#include "atomic_u64.hpp"
#include "rw_spinlock.hpp"

#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/ints.hpp>
#include <base/ref.hpp>

#include <array>
#include <optional>

namespace concurrent {

	/**
	 * @note: for now it uses the std::optional trick, to avoid
	 * Dealing with object lifetimes.
	 * For now can't reuse memory of freed objects.
	 *
	 * @tparam BLOCK_SIZE -- Number of objects to allocate in one block
	 */
	template<class T, u64 BLOCK_SIZE = 2'048>
	class ConcurrentSingleTypeAllocator final {
		/**
		 * Storage for one object. Always stable in memory.
		 *
		 * @note Can be later changed to non-optional, but then the allocator
		 * has to deal with object lifetimes.
		 */
		struct Storage final {
			std::optional<T> value;
		};

		struct Buffer final {
			std::array<Storage, BLOCK_SIZE> items;
		};

		struct BufferIndex final {
			u64 buffer_idx;
			u64 item_idx;
		};

		constexpr static BufferIndex toBufferIndex(u64 idx) {
			return BufferIndex{
				.buffer_idx = idx / BLOCK_SIZE,
				.item_idx   = idx % BLOCK_SIZE,
			};
		}

		Ref<Storage> getNextFreeStorage() noexcept {
			// this will be the index of the item:
			u64 idx = next_free_idx.inc();

			BufferIndex bidx = toBufferIndex(idx);

			MRef<Storage> result = nullptr;

			// now we need to ensure the buffer exists
			// buffers.withReadLock([&](Ref<BufferList> b) {
			//     if (bidx.buffer_idx < b->size()) {
			//         result = MRef<Storage>(&b->at(bidx.buffer_idx)->items[bidx.item_idx]);
			//     }
			// });
			{
				WithReadLock lock(&buffer_lock);
				if (bidx.buffer_idx < buffers.size()) {
					result = MRef<Storage>(&buffers.at(bidx.buffer_idx)->items[bidx.item_idx]);
				}
			}

			if (result.toOpt().has_value()) return result.toOpt().value();

			// buffers.withWriteLock([&](Ref<BufferList> b) {
			// 	while (b->size() <= bidx.buffer_idx) b->emplace_back(makeBox<Buffer>());

			// 	result = MRef<Storage>(&b->at(bidx.buffer_idx)->items[bidx.item_idx]);
			// });
			{
				WithWriteLock lock(&buffer_lock);
				while (buffers.size() <= bidx.buffer_idx) buffers.emplace_back(makeBox<Buffer>());

				result = MRef<Storage>(&buffers.at(bidx.buffer_idx)->items[bidx.item_idx]);
			}

			return result.toOpt().value();
		}


	public:
		template<typename... Args>
		Ref<T> allocateEmplace(Args&&... args) {
			auto storage = getNextFreeStorage();
			CORE_ASSERT(not storage->value.has_value(), "Storage already in use");
			storage->value.emplace(std::forward<Args>(args)...);
			return Ref<T>(&storage->value.value());
		}

		void free(Ref<T>) {
			// Implementation goes here
			// we will either have to return some kind of handle from allocateEmplace
			// or we will have to search for the item
			// handle is probably better, but for now, just panic
			CORE_PANIC("Not implemented");
		}

	private:
		using BufferList = std::vector<Box<Buffer>>;
		BufferList buffers;
		RWSpinLock buffer_lock;

		// WithReadLockObject<BufferList> buffers;

		// AtomicU64 buffer_count = 0;
		AtomicU64 next_free_idx = 0;
	};
}
