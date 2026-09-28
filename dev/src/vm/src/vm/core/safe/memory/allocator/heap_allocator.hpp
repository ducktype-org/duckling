#pragma once
#include "allocator.hpp"
#include "block_data.hpp"

#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/memory/entry_count.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/type.hpp>

namespace vm {

	template<typename EntryT>
	inline EntryT* heapAllocOrThrow(u64 size) {
		try {
			return new EntryT[size];
		} catch (const std::bad_alloc&) { throw exceptions::VMMemoryAllocationError(); }
	}

	template<typename EntryT>
	struct BlockData;

	template<typename EntryT>
	class HeapAllocator final: public IAllocator<EntryT> {
	public:
		BlockData<EntryT> allocate(TypeCRef type) {
			usize size = entryCountFor<EntryT>(type);
			auto  ptr  = heapAllocOrThrow<EntryT>(size);
			return BlockData<EntryT>{ type,
				                      base::TypedModRawView<EntryT>{ ptr, size },
				                      Ref<IAllocator<EntryT>>{ this } };
		}

		/**
		 * @brief Allocates a contiguous data portion for a dynamic table with n elements of type
		 * `inner_type`.
		 * @note Assumes that `table_type` is a dynamic table type with inner type `inner_type`,
		 *  to assign the correct type to the new `BlockData` object.
		 */
		BlockData<EntryT> dynTableAllocateN(TypeCRef table_type, TypeCRef inner_type, u64 n) {
			usize   size = entryCountFor<EntryT>(inner_type) * n;
			EntryT* ptr  = heapAllocOrThrow<EntryT>(size);
			return BlockData<EntryT>{ table_type,
				                      base::TypedModRawView<EntryT>{ ptr, size },
				                      Ref<IAllocator<EntryT>>(this) };
		}

		void deallocate(Ref<BlockData<EntryT>> data) final {
			auto ptr = data->view.getBegin();
			delete[] ptr;
		}
	};
}
