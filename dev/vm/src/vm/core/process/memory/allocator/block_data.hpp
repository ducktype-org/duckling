#pragma once

#include "allocator.hpp"

#include <base/ints.hpp>
#include <base/raw_view.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>

#include <functional>

namespace vm {
	class BlockData final {
	private:
		TypeCRef          element_type;
		base::ModRawView  view;
		Ref<AllocatorABC> allocator;

		friend class Memory;
		friend class HeapAllocator;

	public:
		BlockData(TypeCRef element_type, base::ModRawView view, Ref<AllocatorABC> allocator) noexcept
			  :
			  element_type(element_type),
			  view(view),
			  allocator(allocator) {}
	};
}
