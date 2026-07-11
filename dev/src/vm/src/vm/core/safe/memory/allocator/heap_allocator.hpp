#pragma once
#include "allocator.hpp"
#include "block_data.hpp"

#include <base/misc/raw_view.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/type.hpp>

#include <deque>

namespace vm {

	template<typename EntryT>
	inline EntryT* heapAllocOrThrow(u64 size) {
		try {
			return new EntryT[size];
		} catch (const std::bad_alloc&) { throw exceptions::VMMemoryAllocationError(); }
	}

	template<typename EntryT>
	struct BlockData;

	struct ShadowEntry;

	template<typename EntryT>
	class HeapAllocator final: public IAllocator<EntryT> {
		// It's a mock, it should be replaced with something faster.
		std::deque<base::TypedOwningView<EntryT>> allocated;

	public:
		// @note `size` below is an *element count* passed to heapAllocOrThrow<EntryT>(size) ->
		// new EntryT[size]. For the byte path, Type::getSize() (a byte count) and the element
		// count coincide only because sizeof(std::byte) == 1; for ShadowEntry it's
		// Type::getShadowSize() that already returns the correct element count. Any future EntryT
		// must resolve its own byte-count-to-element-count mapping here explicitly rather than
		// relying on sizeof(EntryT) == 1.
		BlockData<EntryT> allocate(TypeCRef type) {
			usize size;
			if constexpr (std::is_same_v<EntryT, vm::ShadowEntry>)
				size = type->getShadowSize();
			else
				size = type->getSize().asInt();
			auto ptr = heapAllocOrThrow<EntryT>(size);
			allocated.emplace_back(ptr, size);
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
		BlockData<EntryT> dynTableAllocateN(TypeCRef table_type, TypeCRef inner_type, u64 n) {
			usize size;
			if constexpr (std::is_same_v<EntryT, vm::ShadowEntry>)
				size = inner_type->getShadowSize() * n;
			else
				size = inner_type->getSize().asInt() * n;
			EntryT* ptr = heapAllocOrThrow<EntryT>(size);
			allocated.emplace_back(ptr, size);
			return BlockData<EntryT>{ table_type,
				                      base::TypedModRawView<EntryT>{ ptr, size },
				                      Ref<IAllocator<EntryT>>(this) };
		}

		void deallocate(Ref<BlockData<EntryT>> data) final {
			auto ptr = data->view.getBegin();
			for (auto it = allocated.begin(); it != allocated.end(); ++it) {
				if (it->getBegin() == ptr) {
					allocated.erase(it);
					return;
				}
			}
		}
	};
}
