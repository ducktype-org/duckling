// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <vm/core/extc/extc.hpp>

namespace vm {
	/**
	 * @brief Calls an external C function, turning any exception it throws into an `ExtCError`.
	 * Only `KillProcessException` passes through.
	 */
	ExtCReturn invokeExtCFunction(ExtCValueFunctionPtr function, ExtCContext& ctx, ExtCArgs args);

	/** @copydoc invokeExtCFunction */
	ExtCRawReturn invokeExtCFunction(
		ExtCRawFunctionPtr function, ExtCContext& ctx, ExtCRawArgs args, ExtCRawResults results
	);
}
