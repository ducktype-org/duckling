#pragma once

#include "atomic_u64.hpp"
#include "rw_spinlock.hpp"
#include "manual_object_storage.hpp"

#include <base/box.hpp>
#include <base/exceptions.hpp>
#include <base/ints.hpp>
#include <base/ref.hpp>

#include <array>
#include <optional>
#include <deque>

namespace concurrent {

	template<class T, u64 BLOCK_SIZE = 2'048>
	class SingleThreadedAllocator final {
		/**
		 * Storage for one object. Always stable in memory.
		 *
		 * @note Can be later changed to non-optional, but then the allocator
		 * has to deal with object lifetimes.
		 */
		struct Storage final {
		private:
			// std::optional<T> value; // @OPT-NOTE: optional here adds 20%
			ObjStorage<T> value;
			// T value;
		public:
			
			auto emplace(auto&&... args) {
				return value.construct(std::forward<decltype(args)>(args)...);
				// value = T(std::forward<decltype(args)>(args)...);
				// value = T(std::forward<decltype(args)>(args)...);
			}
			void destroy() { value.destroy(); }
			Ref<T> valueRef() { return value.get(); }
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
	public:

		Ref<T> allocateEmplace(auto&&... args) {
			// this will be the index of the item:
			u64 idx = next_free_idx++;
			BufferIndex bidx = toBufferIndex(idx);

			if (bidx.buffer_idx >= buffers.size()) {
				[[unlikely]]
				buffers.emplace_back();
			}

			Ref storage = &buffers[bidx.buffer_idx].items[bidx.item_idx];

			storage->emplace(std::forward<decltype(args)>(args)...);
			return storage->valueRef();
		}

		void free(Ref<T>) {
			// Implementation goes here
			// we will either have to return some kind of handle from allocateEmplace
			// or we will have to search for the item
			// handle is probably better, but for now, just panic
			CORE_PANIC("Not implemented");
		}

	private:

		u64 next_free_idx = 0;
		std::deque<Buffer> buffers;
	};

	// template<class T, u64 BLOCK_SIZE = 2'048, u64 WORKERS = 8>
	// class ConcurrentSingleTypeAllocatorTake2 final {





	// };
}
