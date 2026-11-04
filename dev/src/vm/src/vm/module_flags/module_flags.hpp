// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

namespace vm {
#ifdef BUILD_TYPE_DEV_DEBUG
	/**
	 * Whether detailed VM logging is enabled.
	 * This is a compile-time constant for performance reasons.
	 */
	constexpr bool ENABLE_VM_DETAIL_LOGGING = true;
#else
	constexpr bool ENABLE_VM_DETAIL_LOGGING = false;
#endif
}
