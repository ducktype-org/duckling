#pragma once

#include <base/pointers/ref.hpp>

namespace compiler::mir {
	struct MirLocal;
	struct MirGlobal;

	/**
	 * @brief Reference to MIR Local variable data.
	 */
	using MirLocalRef = CRef<MirLocal>;

	/**
	 * @brief Mutable reference to MIR Local variable data.
	 * Used in the lowering process only.
	 */
	using MirLocalMutRef = Ref<MirLocal>;

	/**
	 * @brief Reference to MIR Global variable data.
	 */
	using MirGlobalRef = CRef<MirGlobal>;

	/**
	 * @brief Mutable reference to MIR Global variable data.
	 * Used in the lowering process only.
	 */
	using MirGlobalMutRef = Ref<MirGlobal>;
}
