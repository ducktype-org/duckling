// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <vm/api/data/process_info.hpp>
#include <vm/bytecode/bytecode.hpp>

namespace compiler::helios::comptime_ops {
	/**
	 * @brief Generates the collection of VM code and external bindings required for compile-time
	 * type operations.
	 *
	 * It constructs a code collection that includes:
	 * 1. **Global Data**: specifically `comptime_query_ctx`, which holds the opaque pointer to
	 * the compiler's `query::Context`, used to call the Type System
	 * 2. **Initialization Functions**: `comptime_set_ctx`, used to inject the C++ query context
	 *    into DVMs memory.
	 * 3. **External C Functions**: A set of external C++ function available for calling from the VM.
	 *
	 *
	 * @param pid The Process ID (PID) of the target VM instance. This is required to associate
	 *            the external C functions with the correct VM memory/process context.
	 *
	 * @return vm::code::CodeCollection containing the necessary globals, functions, and external
	 * function definitions ready to be loaded into the VM.
	 */
	vm::code::CodeCollection getComptimeTypeOperations(vm::PID pid);
}
