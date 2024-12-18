#pragma once

#include <functional>
#include <base/ints.hpp>
#include <base/raw_view.hpp>

#include <core/process/type_metadata/definitions.hpp>
#include "allocator.hpp"

namespace vm {
	class BlockData {
	private:
		TypeCRef          element_type;
		base::ModRawView  view;
		Ref<AllocatorABC> allocator;

		friend class Memory;
		friend class HeapAllocator;

	public:
		BlockData(
			TypeCRef element_type_, base::ModRawView view_, Ref<AllocatorABC> allocator_
		) noexcept:
			  element_type(element_type_),
			  view(view_),
			  allocator(allocator_) {}
	};
}
