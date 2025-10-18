#pragma once

#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

namespace vm {
	struct BlockData;

	class AllocatorABC {
	public:
		virtual ~AllocatorABC()                 = default;
		virtual void deallocate(Ref<BlockData>) = 0;
	};

}
