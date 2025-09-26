#pragma once

#include "atomic_u64.hpp"
#include "manual_object_storage.hpp"
#include "rw_spinlock.hpp"
#include "worker.hpp"

#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/ints.hpp>
#include <base/ref.hpp>

#include <array>
#include <deque>
#include <list>
#include <optional>

namespace concurrent {

	template<class T, u64 BLOCK_SIZE = 2'048>
	class SingleThreadedAllocator final {
		using StorageT = ObjStorage<T>;

		struct Buffer final {
			std::array<StorageT, BLOCK_SIZE> items;
		};

		struct BufferIndex final {
			u64 buffer_idx = 0;
			u64 item_idx   = 0;
		};

		constexpr static BufferIndex toBufferIndex(u64 idx) {
			return BufferIndex{
				.buffer_idx = idx / BLOCK_SIZE,
				.item_idx   = idx % BLOCK_SIZE,
			};
		}

	public:
		SingleThreadedAllocator() { buffers.emplace_back(makeBox<Buffer>()); }

		Ref<T> allocateEmplace(auto&&... args) {
			// @TODO: add a lot of assertions


			// this will be the index of the item:
			// u64 idx = next_free_idx++;
			// BufferIndex bidx = toBufferIndex(idx);

			next_free_idx.item_idx++;

			if (next_free_idx.item_idx >= BLOCK_SIZE) {
				[[unlikely]] next_free_idx.item_idx = 0;
				next_free_idx.buffer_idx++;
				buffers.emplace_back(makeBox<Buffer>());
			}

			// if (bidx.buffer_idx >= buffers.size()) {
			// 	[[unlikely]]
			// 	buffers.emplace_back(makeBox<Buffer>());
			// }

			Ref storage = &buffers[next_free_idx.buffer_idx]->items[next_free_idx.item_idx];

			storage->construct(std::forward<decltype(args)>(args)...);

			return storage->get();
		}

		void free(Ref<T>) {
			// Implementation goes here
			// we will either have to return some kind of handle from allocateEmplace
			// or we will have to search for the item
			// handle is probably better, but for now, just panic
			CORE_PANIC("Not implemented");
		}

	private:
		BufferIndex              next_free_idx;
		std::vector<Box<Buffer>> buffers;
	};

	/**
	 * Concurrent allocator for a single type.
	 * @TODO: free operations, reuse freed items, balance pools between workers
	 */
	template<class T, u64 BLOCK_SIZE = 2'048, u64 WORKERS = 8>
	class ConcurrentSingleTypeAllocatorTake2 final {
		std::array<SingleThreadedAllocator<T, BLOCK_SIZE>, WORKERS> allocators;

	public:
		Ref<T> allocateEmplace(Ref<WorkerData> worker, auto&&... args) noexcept {
			// @TODO: change to core assert:
			// CORE_ASSERT(worker->id < WORKERS, "Too many workers for the allocator");

			return allocators.at(worker->id).allocateEmplace(std::forward<decltype(args)>(args)...);
		}
	};

	// };
}
