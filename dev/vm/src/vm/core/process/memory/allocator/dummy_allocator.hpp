#pragma once

#include "allocator.hpp"
#include "block_data.hpp"

#include <base/ints.hpp>
#include <base/ref.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>

namespace vm {
	class DummyAllocator final: public AllocatorABC {
	public:
		BlockData allocate(TypeCRef type, Ref<std::byte> data) {
			auto size = type->getSize();
			return BlockData{ type, base::ModRawView{ data.get(), size }, this };
		}

		void deallocate(Ref<BlockData>) final {
			// Nothing here..
		}
	};
}
