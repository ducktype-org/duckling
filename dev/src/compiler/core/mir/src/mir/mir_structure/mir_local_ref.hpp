#pragma once

#include <base/ref.hpp>

namespace compiler::mir {
	struct MirLocal;
	struct MirGlobal;

	/**
	 * @brief Reference to MIR Local variable data.
	 */
	using LocalRef = CRef<MirLocal>;

	/**
	 * @brief Mutable reference to MIR Local variable data.
	 * Used in the lowering process only.
	 */
	using MutLocalRef = Ref<MirLocal>;
}
