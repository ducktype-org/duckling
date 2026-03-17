#pragma once
#include "allocator.hpp"
#include "block_data.hpp"

#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/process/exceptions.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>

#include <deque>

namespace vm {

	inline std::byte* heapAllocOrThrow(u64 size) {
		try {
			return new std::byte[size];
		} catch (const std::bad_alloc&) { throw exceptions::VMMemoryAllocationError(); }
	}

	struct BlockData;

	class HeapAllocator final: public AllocatorABC {
		// It's a mock, it should be replaced with something faster.
		std::deque<base::OwningView> allocated;

	public:
		BlockData allocate(TypeCRef type) {
			auto             size = type->getSize().asInt();
			auto             ptr  = heapAllocOrThrow(size);
			base::OwningView view{ ptr, size };
			allocated.push_back(std::move(view));
			return BlockData{ type, base::ModRawView{ ptr, size }, Ref<AllocatorABC>{ this } };
		}

		/**
		 * @brief Allocates a contiguous data portion for a dynamic table with n elements of type
		 * `inner_type`.
		 * @note Assumes that `table_type` is a dynamic table type with inner type `inner_type`,
		 *  to assign the correct type to the new `BlockData` object.
		 */
		BlockData dynTableAllocateN(TypeCRef table_type, TypeCRef inner_type, u64 n) {
			auto       size = inner_type->getSize().asInt() * n;
			std::byte* ptr  = heapAllocOrThrow(size);
			allocated.emplace_back(ptr, size);
			return BlockData{ table_type, base::ModRawView{ ptr, size }, Ref<AllocatorABC>(this) };
		}

		void deallocate(Ref<BlockData> data) final {
			auto ptr = data->view.getBegin();
			for (auto it = allocated.begin(); it != allocated.end(); ++it) {
				if (it->view().getBegin() == ptr) {
					allocated.erase(it);
					return;
				}
			}
		}
	};
}
