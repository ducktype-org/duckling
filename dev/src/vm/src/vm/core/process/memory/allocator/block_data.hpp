#pragma once

#include "allocator.hpp"

#include <base/types/ints.hpp>
#include <base/raw_view.hpp>

#include <vm/core/process/type_metadata/definitions.hpp>

namespace vm {
	struct BlockData final {
		TypeCRef          element_type;
		base::ModRawView  view;
		Ref<AllocatorABC> allocator;

		BlockData(TypeCRef element_type, base::ModRawView view, Ref<AllocatorABC> allocator) noexcept
			  :
			  element_type(element_type),
			  view(view),
			  allocator(allocator) {}
	};
}
