#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout.hpp>
#include <helios/scope_id.hpp>

#include <string_id/string_id.hpp>

#include <vector>

namespace compiler::repl {
	/**
	 * @brief Resolve root scope of a script module main file.
	 *
	 * @param ctx Active query context.
	 * @param main_module_id Module whose main source file root scope should be used.
	 */
	helios::ScopeID queryScriptMainRootScope(query::Context& ctx, frontend::ModuleID main_module_id);

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
	 * - `script_id` is used for stable generated-symbol identity/hash.
	 * - It does not define entrypoint naming; naming comes from `.name = "main"`.
	 *
	 * This function expects already-resolved root scope of the script main module.
	 *
	 * @param ctx Query context used for symbol generation and declaration queries.
	 * @param script_id Stable per-script identity stored in generated symbol metadata.
	 *        It is used for generated-symbol key/hash identity (query/cache stability),
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
