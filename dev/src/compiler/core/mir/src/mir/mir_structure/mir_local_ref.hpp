// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
