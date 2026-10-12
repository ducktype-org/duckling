// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/except/exceptions.hpp>

#include <vm/utils/interpret.hpp>

namespace compiler::backend_vm::internal {
	/**
	 * @brief Safely transforms an immediate value of type T to `u64` which is then used in DVMs
	 * opcodes. This basically means a bit_cast.
	 */
	template<class T>
	constexpr u64 translateToU64(T value) {
		if constexpr (sizeof(T) == 8)
			return vm::safeReadObjectBytes<u64>(value);
		else if constexpr (sizeof(T) == 4)
			return static_cast<u64>(vm::safeReadObjectBytes<u32>(value));
		else if constexpr (sizeof(T) == 2)
			return static_cast<u64>(vm::safeReadObjectBytes<u16>(value));
		else if constexpr (sizeof(T) == 1)
			return static_cast<u64>(vm::safeReadObjectBytes<u8>(value));
		else
			CORE_PANIC("Unsupported immediate size: ", sizeof(T));
	}
}
