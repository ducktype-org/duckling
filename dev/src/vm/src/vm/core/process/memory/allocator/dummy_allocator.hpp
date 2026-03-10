#pragma once

#include "allocator.hpp"
#include "block_data.hpp"

#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>

namespace vm {
	class DummyAllocator final: public AllocatorABC {
	public:
		BlockData allocate(TypeCRef type, Ref<byte> data) {
			auto size = type->getSize().asInt();
			return BlockData{ type, base::ModRawView{ data.get(), size }, this };
		}

		void deallocate(Ref<BlockData>) final {
			// Nothing here..
		}
	};
}
