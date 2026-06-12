#pragma once

#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

namespace vm {
	template<typename EntryT>
	struct BlockData;

	template<typename EntryT>
	class IAllocator {
	public:
		virtual ~IAllocator()                           = default;
		virtual void deallocate(Ref<BlockData<EntryT>>) = 0;
	};

	using AllocatorABC = IAllocator<std::byte>;

}
