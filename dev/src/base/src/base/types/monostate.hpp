// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

namespace base {
	/**
	 * @brief A utility type that has only one possible value.
	 * @note It is useful for example as compile-time marker members inside classes.
	 */
	struct Monostate final {};
}
