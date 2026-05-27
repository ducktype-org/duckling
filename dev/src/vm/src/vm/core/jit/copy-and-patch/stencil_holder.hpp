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

	// TODO: use non-jitable.hpp to get the jitable
	[[nodiscard]] const std::array<StencilData, low::microInstrCount()>& stencilsData();
}
