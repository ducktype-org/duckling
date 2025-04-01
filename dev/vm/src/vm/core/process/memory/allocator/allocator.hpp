#pragma once

#include <base/ints.hpp>
#include <base/ref.hpp>

namespace vm {
	class BlockData;

	class AllocatorABC {
	public:
		virtual ~AllocatorABC()                 = default;
		virtual void deallocate(Ref<BlockData>) = 0;
	};

}
