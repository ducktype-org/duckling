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
	// These wrappers are deliberately defined out-of-line in stencil_holder.cpp: they form the
	// opaque boundary that keeps the embedded stencil binary confined to that single TU, instead
	// of being compiled into every includer of this header.
	byte* relocate(const StencilData& stencil_data, byte* new_address);
	[[nodiscard]] std::span<const byte> stencilsBinary(const StencilData& stencil_data);

	enum class SpecialStencils : u64 {
		JumpIf = low::microInstrCount(),
		JumpIfNot,
		Jump,
		CallAddr,

		StencilsCount,
	};

	constexpr size_t STENCIL_COUNT = std::to_underlying(SpecialStencils::StencilsCount);

	[[nodiscard]] const std::array<StencilData, STENCIL_COUNT>& stencilsData();
}
