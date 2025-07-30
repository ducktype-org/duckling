#pragma once
#include "allocator.hpp"
#include "block_data.hpp"

#include <base/ints.hpp>
#include <base/raw_view.hpp>
#include <base/ref.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>

#include <deque>

namespace vm {

	struct BlockData;

	class HeapAllocator final: public AllocatorABC {
		// It's a mock, it should be replaced with something faster.
		std::deque<base::OwningView> allocated;

	public:
		BlockData allocate(TypeCRef type) {
			auto             size = u64(type->getSize());
			auto             ptr  = new std::byte[size];
			base::OwningView view{ ptr, size };
			allocated.push_back(std::move(view));
			return BlockData{ type, base::ModRawView{ ptr, size }, Ref<AllocatorABC>{ this } };
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
