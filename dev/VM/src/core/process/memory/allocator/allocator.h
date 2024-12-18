#pragma once

#include "base/ref.hpp"
#include <base/ints.hpp>
namespace vm {
	class BlockData;
	class AllocatorABC {
	public:
		virtual ~AllocatorABC()                        = default;
		virtual void deallocate(Ref<BlockData>) = 0;
	};

}
