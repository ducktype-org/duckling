// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout.hpp>
#include <helios/scope_id.hpp>

#include <string_id/string_id.hpp>

#include <vector>

namespace compiler::repl {
	/**
	 * @brief Resolve the canonical root scope used for generated script `main`.
	 *
	 * Script compilation creates a chain of synthetic REPL modules, one per statement,
	 * where each next module points to the previous one as REPL parent. The last module
	 * in that chain represents the full accumulated script context.
	 *
	 * Even though these modules are synthetic, each of them still has a real main source file:
	 * createSyntheticChainedStatementModule() assigns one via createRandomVirtualFile(input).
	 * That virtual file is parsed into PST just like regular files, so querying
	 * queryRootScopeOfMainModuleFile() is the correct way to obtain the module's top-level scope.
	 *
	 * This helper returns the primary scope for that module's main source file root PST node.
	 * Using that exact scope lets the generated wrapper symbol be treated as global `main`
	 * by ABI/mangling logic (through global-function checks based on scope depth).
	 *
	 * @param ctx Active query context.
	 * @param terminal_module_id The terminal module in the statement chain.
	 * @return Root scope of the module's main source file, suitable for generated script `main`.
	 */
	helios::ScopeID queryScriptMainRootScope(
		query::Context& ctx, frontend::ModuleID terminal_module_id
	);

	/**
	 * @brief Build synthetic global `main` wrapper for LLVM script compilation.
	 *
	 * The returned function calls wrapper symbols in source order and returns i64 zero.
	 *
	 * Why this is named `main`:
	 * - The produced LLVM object is linked into a runnable executable, which needs
	 *   a process entry function named `main`.
	 * - In QuerySymbolABI (see symbol_abi.cpp), global `main` is mapped to CAbi with:
	 *     if (name(key) == "main" && isGlobalFun(key)) return CAbi{};
	 * - CAbi causes mangling to be skipped, so the linker/runtime sees plain `main`.
	 *
	 * Why script_id exists:
	 * - `script_id` contributes a stable component of generated-symbol identity/hash.
	 * - The full unstable hash for ScriptMainWrapper is still scope-dependent.
	 * - It does not define entrypoint naming; naming comes from `.name = "main"`.
	 *
	 * This function expects already-resolved root scope of the script main module.
	 *
	 * @param ctx Query context used for symbol generation and declaration queries.
	 * @param script_id Stable per-script identity stored in generated symbol metadata.
	 *        It contributes one component of generated-symbol key/hash identity,
	 *        not for choosing the emitted entrypoint name.
	 * @param main_scope Root scope where generated `main` should be placed.
	 * @param wrapper_symbols Ordered wrapper call list to execute from generated `main`.
	 */
	helios::HOUTFunction buildScriptMainWrapper(
		query::Context&                   ctx,
		base::StrID                       script_id,
		helios::ScopeID                   main_scope,
		const std::vector<helios::SymID>& wrapper_symbols
	);

}  // namespace compiler::repl
