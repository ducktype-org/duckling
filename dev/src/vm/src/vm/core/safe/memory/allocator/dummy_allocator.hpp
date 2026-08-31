#pragma once

#include "allocator.hpp"
#include "block_data.hpp"

#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/type.hpp>

namespace vm {
	template<typename EntryT>
	class DummyAllocator final: public IAllocator<EntryT> {
	public:
		// @TODO: #3447 Same bytes-vs-entries conflation as `HeapAllocator::allocate`: `size` is a
		// byte count from `Type::getSize()` but is used as the entry count of the view below.
		// Correct only while `sizeof(EntryT) == 1`, which the `static_assert` pins.
		BlockData<EntryT> allocate(TypeCRef type, Ref<EntryT> data) {
			static_assert(sizeof(EntryT) == 1);
			usize size = type->getSize().asInt();
			return BlockData<EntryT>{ type,
				                      base::TypedModRawView<EntryT>{ data.get(), size },
				                      this };
		}

		void deallocate(Ref<BlockData<EntryT>>) final {
			// Nothing here..
		}
	};
}
