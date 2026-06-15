/**
 * @file stencil_holder.cpp
 * @brief Holds a singleton loaded stencils, providing the interface but with limited data
 * transparency.
 */

#include "stencil_holder.hpp"

#define UNDER_LINTER __has_include(<stencils-nm>)

namespace vm::jit::cnp {
	[[nodiscard]] auto& getLoadedStencils() {
		// NOLINTBEGIN
		PUSH_DIAGNOSTIC
		ALLOW_EXTENSIONS
		static constexpr char binary[] = {
#embed "stencils-so"
		};
		POP_DIAGNOSTIC
		// NOLINTEND

		static auto stencils = Stencils{
// Linter doesn't actually build stencils-nm so it would be unavailable.
#if UNDER_LINTER
			.stencils_binary = std::bit_cast<std::array<std::byte, sizeof(binary)>>(binary),
			.stencils_data =
	#include <stencils-nm>
#endif
		};

		static auto loaded_stencils = std::move(stencils).load();
		return loaded_stencils;
	}

	byte* relocate(const StencilData& stencil_data, byte* new_address) {
		return getLoadedStencils().relocate(stencil_data, new_address);
	}

	[[nodiscard]] std::span<const byte> stencilsBinary(const StencilData& stencil_data) {
		return getLoadedStencils().stencilsBinary(stencil_data);
	}

	[[nodiscard]] const std::array<StencilData, STENCIL_COUNT>& stencilsData() {
// Linter doesn't actually build stencils-nm so the array is empty.
#if UNDER_LINTER
		return getLoadedStencils().stencilsData();
#else
		CORE_UNREACHABLE();
#endif
	}
}

#undef UNDER_LINTER
