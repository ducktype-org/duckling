// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <backends/dvm/repl_lowering.hpp>
#include <helios/hout/hout.hpp>

#include <query_framework/context/context.hpp>

#include <vm/core/process/interface_types.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace compiler::repl {
	/**
	 * @brief Compile a HOUT unit to a standalone DVM code chunk using persistent REPL lowering
	 * context.
	 *
	 * This is the same lowering path used by REPL incremental execution, but without sending
	 * the code to a running VM process.
	 */
	std::expected<vm::code::CodeCollection, std::string> compileHOUTUnitToDVMCode(
		query::Context&                 ctx,
		const helios::HOUTUnit&         hout_unit,
		std::string_view                module_name,
		backend_vm::ReplDVMCodeBuilder& lowering_context
	);

	/**
	 * @brief Compile a HOUT unit to DVM bytecode and load it into a running DVM process.
	 *
	 * @param ctx          Active query context for LIR compilation queries.
	 * @param hout_unit    The HOUT unit to compile.
	 * @param module_name  Used for identification and symbol resolution.
	 * @param pid          Process ID of the target DVM instance.
	 * @param lowering_context Persistent lowering context for REPL statement compilation.
	 *
	 * @note This function does NOT call setContext() or invalidateContext() on lowering_context.
	 *       The caller is responsible for managing the context lifecycle.
	 *
	 * @pre The lowering_context must have an active query::Context set via setContext()
	 *      BEFORE calling this function.
	 * @pre The active context in lowering_context MUST be the same object as @c ctx.
	 *      This is an enforced runtime invariant (asserted by the lowering path).
	 *
	 * @return Success or error message on failure.
	 */
	std::expected<void, std::string> compileAndLoad(
		query::Context&                 ctx,
		const helios::HOUTUnit&         hout_unit,
		std::string_view                module_name,
		vm::PID                         pid,
		backend_vm::ReplDVMCodeBuilder& lowering_context
	);

	/**
	 * @brief Compile the standard library to in-memory DVM bytecode and load it into a running
	 * DVM process.
	 *
	 * Does nothing (and succeeds) when no standard library packages are registered.
	 *
	 * @param ctx              Active query context for LIR compilation queries.
	 * @param pid              Process ID of the target DVM instance.
	 * @param lowering_context Persistent lowering context for REPL statement compilation.
	 *
	 * @note Like compileAndLoad(), this does NOT manage the context lifecycle of
	 * @p lowering_context; the caller must have called setContext() with @p ctx first.
	 *
	 * @return Success or error message on failure.
	 */
	std::expected<void, std::string> preloadStandardLibrary(
		query::Context& ctx, vm::PID pid, backend_vm::ReplDVMCodeBuilder& lowering_context
	);

	/**
	 * @brief Execute a previously loaded function on the DVM and capture its return value
	 *
	 * Runs any previously loaded function on the DVM, waits for completion, and extracts
	 * the return value as a formatted string. Primarily used for executing REPL expression
	 * wrappers, but can execute any loaded function with a supported return type.
	 *
	 * @param pid Process ID of the target DVM instance
	 * @param func_name Mangled name of the function to execute
	 * @param return_type Return type of the function, used to format the output
	 * @return Formatted output string on success, error message on failure
	 */
	std::expected<std::string, std::string> executeFunctionAndCaptureResult(
		vm::PID pid, std::string_view func_name, const tsh::SymbolType<>& return_type
	);

}  // namespace compiler::repl
