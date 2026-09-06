/**
 * @file stencil_holder.cpp
 * @brief Holds a singleton loaded stencils, providing the interface but with limited data
 * transparency.
 */

#include "stencil_holder.hpp"

#define NOT_UNDER_LINTER __has_include(<stencils-cpp>)

namespace vm::jit::cnp {
	[[nodiscard]] auto& getLoadedStencils() {
		// NOLINTBEGIN
		PUSH_DIAGNOSTIC
		ALLOW_EXTENSIONS
		static constexpr char binary[] = {
// Linter doesn't actually build stencils-so so it would be unavailable.
#if NOT_UNDER_LINTER
	#embed "stencils-so"
#else
			0
#endif
		};
		POP_DIAGNOSTIC
		// NOLINTEND

		static auto stencils = Stencils{
// Linter doesn't actually build stencils-cpp so it would be unavailable.
#if NOT_UNDER_LINTER
			.stencils_binary = std::bit_cast<std::array<std::byte, sizeof(binary)>>(binary),
			.stencils_data =
	#include <stencils-cpp>
#endif
		};

		static auto loaded_stencils = std::move(stencils).load().value();
		return loaded_stencils;
	}

	byte* relocate(const StencilData& stencil_data, byte* new_address) {
		return getLoadedStencils().relocate(stencil_data, new_address);
	}

	[[nodiscard]] std::span<const byte> stencilsBinary(const StencilData& stencil_data) {
		return getLoadedStencils().stencilsBinary(stencil_data);
	}

	[[nodiscard]] const std::array<StencilData, STENCIL_COUNT>& stencilsData() {
// Linter doesn't actually build stencils-cpp so the array is empty.
#if NOT_UNDER_LINTER
		return getLoadedStencils().stencilsData();
#else
		CORE_UNREACHABLE();
#endif
	}
}

#undef NOT_UNDER_LINTER
