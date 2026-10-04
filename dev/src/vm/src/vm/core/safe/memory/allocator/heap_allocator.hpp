#pragma once
#include "allocator.hpp"
#include "block_data.hpp"

#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/type.hpp>

namespace vm {
	template<typename EntryT>
	EntryT* heapAllocOrThrow(u64 size) {
		try {
			return new EntryT[size];
		} catch (const std::bad_alloc&) { throw exceptions::VMMemoryAllocationError(); }
	}

	template<typename EntryT>
	struct BlockData;

	template<typename EntryT>
	class HeapAllocator final: public IAllocator<EntryT> {
	public:
		// @TODO: #3447 Resolve the bytes-vs-entries unit conflation below before instantiating
		// the memory module with a non-byte `EntryT`, then drop the `static_assert`.
		// @note `size` here is a byte count (from Type::getSize()) but is passed to
		// heapAllocOrThrow<EntryT>(size) -> new EntryT[size], where the argument is an element
		// count. The two only coincide while sizeof(EntryT) == 1, which the static_assert below
		// enforces. This path is intentionally byte-only for now; widening EntryT will need the
		// bytes-vs-entries distinction resolved here deliberately, instead of silently
		// over-allocating.
		BlockData<EntryT> allocate(TypeCRef type) {
			static_assert(sizeof(EntryT) == 1);
			usize size = type->getSize().asInt();
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
		 * @note Same byte-count/element-count aliasing as `allocate()` above, see its note.
		 */
		BlockData<EntryT> dynTableAllocateN(TypeCRef table_type, u64 n) {
			static_assert(sizeof(EntryT) == 1);
			usize   size = table_type->getInnerType().value()->getSize().asInt() * n;
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
