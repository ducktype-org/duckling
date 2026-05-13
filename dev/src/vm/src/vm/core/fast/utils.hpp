#pragma once

#include <base/types/ints.hpp>

#include "vm/core/fast/program/program.hpp"
#include <vm/core/fast/program/instructions/executable.hpp>
#include <vm/core/fast/program/instructions/relocatable.hpp>

namespace vm::fast {
	constexpr u64 encodeStackOffsetToRelocatablePlace(u64 offset) {
		return offset;  // Just return the offset, as the highest bit is 0 for stack offsets.
	}

	constexpr u64 encodeGlobalIDToRelocatablePlace(GlobalDataID global_id) {
		return (1ULL << 63) | global_id.asInt();  // Set the highest bit to 1 for global IDs.
	}

	constexpr u64 encodeRelocatableToExecutable(
		u64 relocatable_place, fast::GlobalData* global_data_begin
	) {
		const bool is_global = relocatable_place >> 63;
		if (is_global) {
            const u64 global_id = relocatable_place & ~(1ULL << 63);  // Mask out the highest bit to get the global ID.
            fast::GlobalData* global_data_ptr = global_data_begin + global_id;  // Get the pointer to the global data.
			return reinterpret_cast<u64>(global_data_ptr);  // Return the pointer as a u64.
		} else {
			return reinterpret_cast<std::byte*>(relocatable_place);
		}
	}

	constexpr std::byte* decodeExecutablePlace(u64 encoded) {
		return reinterpret_cast<std::byte*>(encoded);
	}
}
