#pragma once

#include <base/ints.hpp>
#include <base/ref.hpp>

#include "allocator.hpp"
#include "block_data.hpp"
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>

namespace vm {
	class StackAllocator final: public AllocatorABC {
	public:
		BlockData allocate(TypeCRef type, Ref<std::byte> data) {
			auto size = type->getSize();
			return BlockData{ type,
				              base::ModRawView{ data.get(), size },
				              Ref<AllocatorABC>{ this } };
		}

		void deallocate(Ref<BlockData>) final {
			// Nothing here..
		}
	};
}
