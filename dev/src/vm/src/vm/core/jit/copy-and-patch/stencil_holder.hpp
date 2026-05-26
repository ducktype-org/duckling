/**
 * @file stencil_holder.hpp
 * @brief Breaks the dependency of copy-and-patch compiler on binary stencils, by providing an opaque interface.
 */

#include <cstdint>
#include <span>

#include "stencils/import_stencils.hpp"

namespace vm::jit::cnp {
    byte* relocate(const StencilData& stencil_data, byte* new_address);
    [[nodiscard]] std::span<const byte> stencilsBinary(const StencilData& stencil_data);
    [[nodiscard]] std::array<StencilData, 324>& stencilsData(); // TODO: use non-jitable.hpp to get the jitable
}