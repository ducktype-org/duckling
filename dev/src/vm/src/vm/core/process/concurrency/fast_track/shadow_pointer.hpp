#pragma once

#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

namespace vm {

	struct ShadowEntry;
	struct ShadowPointer;

	template<typename EntryT>
	class BasicBlock;

	using ShadowBlock = BasicBlock<ShadowEntry>;
	using ShadowPointerBlock = BasicBlock<ShadowPointer>;

	/**
	 * @brief Redesigned ShadowPointer as a "Coordinate Pack".
	 * Allows O(1) access to shadow data and nested shadow pointers
	 * on both stack and heap.
	 */
	struct ShadowPointer {
		MRef<ShadowBlock>        shadow_block = nullptr;
		MRef<ShadowPointerBlock> shadow_pointer_block = nullptr;
		u32                      logical_offset = 0;       // Offset from shadow_block for the current element
		u32                      pointer_offset = 0;       // Offset from shadow_pointer_block for the current element

		[[nodiscard]] ShadowEntry* data_base() const;
		[[nodiscard]] ShadowPointer* pointer_base() const;

		constexpr bool isNull() const { return shadow_block == nullptr; }
		operator bool() const { return !isNull(); }

		static constexpr ShadowPointer null() { return {}; }
	};

}
