#pragma once

#include <base/types/ints.hpp>

namespace vm {

	struct ShadowEntry;

	/**
	 * @brief Redesigned ShadowPointer as a "Coordinate Pack".
	 * Allows O(1) access to shadow data and nested shadow pointers
	 * on both stack and heap.
	 */
	struct ShadowPointer {
		ShadowEntry*   data_base    = nullptr; // Base address for R/W logical entries
		ShadowPointer* pointer_base = nullptr; // Base address for nested shadow pointers
		u32            logical_idx  = 0;       // Offset from data_base for the current element
		u32            pointer_idx  = 0;       // Offset from pointer_base for the current element

		constexpr bool isNull() const { return data_base == nullptr; }
		operator bool() const { return !isNull(); }

		static constexpr ShadowPointer null() { return {}; }
	};

}
