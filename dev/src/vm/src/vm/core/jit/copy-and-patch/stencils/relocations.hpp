// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/config/target_info.hpp>
#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/types/ints.hpp>

#include <concepts>
#include <cstddef>

MAKE_STRINGIFYABLE_ENUM(vm::jit::cnp, u32, HoleValue,
	InstrPtr,
	Arg0,
	Arg1,
	ContinueFn,
	JmpFn,
	CallFn,
	CallOpcode,
	None
);

namespace vm::jit::cnp {
	struct StencilHole final {
		int       offset;
		HoleValue value;

		template<std::integral T>
		inline void patch(byte* new_addr, T value) const;
	};

#if BASE_TARGET_ARCH_X86 && BASE_TARGET_ARCH_64
	template<std::integral T>
	void StencilHole::patch(byte* new_addr, T value) const {
		*reinterpret_cast<T*>(new_addr + offset) += value;
	}
#else
	#error "Patching unimplemented on your architecture"
#endif
}
