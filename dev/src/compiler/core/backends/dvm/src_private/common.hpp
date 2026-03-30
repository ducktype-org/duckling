#pragma once

#include <base/except/exceptions.hpp>

#include <vm/utils/interpret.hpp>

namespace compiler::backend_vm::internal {
	template<class T>
	u64 translateToU64(T value) {
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
