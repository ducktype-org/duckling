// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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

	using AllocatorABC = IAllocator<byte>;
}
