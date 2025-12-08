#pragma once

#include <base/pointers/ref.hpp>

namespace compiler::mir {
	struct MIRLocal;
	struct MIRGlobal;

	/**
	 * @brief Reference to MIR Local variable data.
	 * This is useful because MIR Locals are owned by MIR Functions, unlike MIR Globals.
	 */
	using MIRLocalRef = CRef<MIRLocal>;

	/**
	 * @brief Mutable reference to MIR Local variable data.
	 * This is useful because MIR Locals are owned by MIR Functions, unlike MIR Globals.
	 * Used in the lowering process only.
	 */
	using MIRLocalMutRef = Ref<MIRLocal>;
}
