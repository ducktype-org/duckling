// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

namespace time_stats {
	/**
	 * If set, time_stats module will collect and output time statistics.
	 * If unset, time_stats module will do nothing, and the collected time statistics will be empty.
	 *
	 * @note It is needed mostly, because time statistics collection can have significant
	 * performance overhead, and we don't want to pay it when we don't need it.
	 */
	constexpr bool ENABLE_TIME_STATS = true;
}
