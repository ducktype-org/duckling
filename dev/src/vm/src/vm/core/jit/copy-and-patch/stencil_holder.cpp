/**
 * @file stencil_holder.cpp
 * @brief Holds a singleton loaded stencils, providing the interface but with limited data
 * transparency.
 */

#include "stencil_holder.hpp"

namespace vm::jit::cnp {
	[[nodiscard]] auto& getLoadedStencils() {
		static auto loaded_stencils = Stencils{
// Linter doesn't actually build stencils-nm so it would be unavailable.
#if __has_include(<stencils-nm>)
	#include <stencils-nm>
#else
	#error "Not found"
#endif
        }.load();
		return loaded_stencils;
	}

	byte* relocate(const StencilData& stencil_data, byte* new_address) {
		return getLoadedStencils().relocate(stencil_data, new_address);
	}

	[[nodiscard]] std::span<const byte> stencilsBinary(const StencilData& stencil_data) {
		return getLoadedStencils().stencilsBinary(stencil_data);
	}

	[[nodiscard]] const std::array<StencilData, low::microInstrCount()>& stencilsData() {
		return getLoadedStencils().stencilsData();
	}
}
