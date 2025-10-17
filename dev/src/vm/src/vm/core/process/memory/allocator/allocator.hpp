#pragma once

#include <base/types/ints.hpp>
#include <base/ref.hpp>

namespace vm {
	struct BlockData;

	class AllocatorABC {
	public:
		virtual ~AllocatorABC()                 = default;
		virtual void deallocate(Ref<BlockData>) = 0;
	};

}
