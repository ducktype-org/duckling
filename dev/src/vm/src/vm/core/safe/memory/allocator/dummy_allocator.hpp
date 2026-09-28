#pragma once

#include "allocator.hpp"
#include "block_data.hpp"

#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/memory/entry_count.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/type.hpp>

namespace vm {
	template<typename EntryT>
	class DummyAllocator final: public IAllocator<EntryT> {
	public:
		BlockData<EntryT> allocate(TypeCRef type, Ref<EntryT> data) {
			usize size = entryCountFor<EntryT>(type);
			return BlockData<EntryT>{ type,
				                      base::TypedModRawView<EntryT>{ data.get(), size },
				                      this };
		}

		void deallocate(Ref<BlockData<EntryT>>) final {
			// Nothing here..
		}
	};
}
