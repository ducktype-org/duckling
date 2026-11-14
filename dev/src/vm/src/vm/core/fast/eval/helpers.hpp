// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "trap.hpp"

#include <base/types/ints.hpp>

namespace vm::fast {
	class ThreadRuntimeState;
	class FastVMThread;

	/**
	 * @brief Out-of-line work of the helper instructions (D-11 in core/fast/readme.md).
	 *
	 * Every helper is `noexcept`, returns a `Trap` and wraps its body in `FAST_HELPER_BOUNDARY`,
	 * so exceptions never leave it. Helpers are `noinline`, also under LTO, so their exception
	 * handling never lands in a handler, and they never touch the instruction pointer or frames.
	 */
	class FastHelpers final {
	public:
		[[gnu::noinline]] static Trap inputI64(
			ThreadRuntimeState& state, FastVMThread& thread, i64& value
		) noexcept;

		[[gnu::noinline]] static Trap outputU64(
			ThreadRuntimeState& state, FastVMThread& thread, u64 value
		) noexcept;
	};
}
