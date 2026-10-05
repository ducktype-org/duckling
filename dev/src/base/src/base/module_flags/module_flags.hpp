// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @brief Global flags of base module.
 * We want base to be stateless, and so
 * all flags used here are compile-time constants.
 * Modify this file to change the flags.
 */

#pragma once

namespace base {

	/**
	 * If set, base will output logs
	 * useful mostly in development and debugging.
	 */
	constexpr bool ENABLE_DEV_LOGS = false;
}
