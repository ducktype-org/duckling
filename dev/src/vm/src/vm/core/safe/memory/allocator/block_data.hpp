#pragma once

#include "allocator.hpp"

#include <base/misc/raw_view.hpp>
#include <base/types/ints.hpp>

#include <vm/core/safe/type_metadata/definitions.hpp>

namespace vm {
	template<typename EntryT>
	struct BlockData final {
		TypeCRef                element_type;
		base::TypedModRawView<EntryT> view;
		Ref<IAllocator<EntryT>> allocator;

		BlockData(
			TypeCRef element_type, base::TypedModRawView<EntryT> view, Ref<IAllocator<EntryT>> allocator
		) noexcept
			  :
			  element_type(element_type),
			  view(view),
			  allocator(allocator) {}
	};
}
