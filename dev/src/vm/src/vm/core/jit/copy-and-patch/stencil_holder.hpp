/**
 * @file stencil_holder.hpp
 * @brief Breaks the dependency of copy-and-patch compiler on binary stencils, by providing an
 * opaque interface.
 */

#pragma once

#include "stencils/import_stencils.hpp"

#include <vm/core/safe/low_program/opcodes.hpp>

#include <cstdint>
#include <span>

namespace vm::jit::cnp {
	byte* relocate(const StencilData& stencil_data, byte* new_address);
	[[nodiscard]] std::span<const byte> stencilsBinary(const StencilData& stencil_data);

	enum class SpecialStencils : u64 {
		Return = low::microInstrCount(),
		JumpIf,
		JumpIfNot,
		Jump,
		CallAddr,

		StencilsCount,
	};

	constexpr size_t STENCIL_COUNT = std::to_underlying(SpecialStencils::StencilsCount);

	[[nodiscard]] const std::array<StencilData, STENCIL_COUNT>& stencilsData();
}
