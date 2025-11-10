#pragma once

#include <base/pointers/ref.hpp>

namespace compiler::mir {
	struct MirLocal;
	struct MirGlobal;

	/**
	 * @brief Reference to MIR Local variable data.
	 * This is useful because MIR Locals are owned by MIR Functions, unlike MIR Globals.
	 */
	using MirLocalRef = CRef<MirLocal>;

	/**
	 * @brief Mutable reference to MIR Local variable data.
	 * This is useful because MIR Locals are owned by MIR Functions, unlike MIR Globals.
	 * Used in the lowering process only.
	 */
	using MirLocalMutRef = Ref<MirLocal>;
}
