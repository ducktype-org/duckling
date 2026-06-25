#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout.hpp>
#include <helios/scope_id.hpp>

#include <string_id/string_id.hpp>

#include <deque>
#include <variant>
#include <vector>

namespace compiler::repl {
	/**
	 * @brief Synthetic-main action that calls an executable-statement wrapper function.
	 */
	struct ScriptMainWrapperCall final {
		helios::SymID wrapper_symbol;
	};

	/**
	 * @brief Synthetic-main action that assigns a global variable its initializer in source order.
	 *
	 * @note `value` references the lowered HOUT initializer expression owned by the module HOUT
	 * (kept alive by the query cache for the whole compilation).
	 */
	struct ScriptMainGlobalInit final {
		helios::SymID            global_symbol;
		CRef<helios::code::Expr> value;
	};

	/**
	 * @brief One sequenced action in the synthetic script `main`, preserving source order.
	 */
	using ScriptMainAction = std::variant<ScriptMainWrapperCall, ScriptMainGlobalInit>;

	/**
	 * @brief Result of deferring a definition module's mutable global initializers.
	 */
	struct NeutralizedScriptModule final {
		/// HOUT unit with mutable global initializers replaced by harmless default initializers.
		helios::HOUTUnit unit;
		/// Real initializers to run from the synthetic `main`, in source order.
		std::vector<ScriptMainAction> deferred_inits;
		/// Backing storage for the default-init globals that `unit` references by CRef; must stay
		/// alive as long as `unit` is used (e.g. while lowering it).
		std::deque<helios::HOUTGlobalData> default_init_storage;
	};

	/**
	 * @brief Defer mutable global variable initialization in a definition module's HOUT.
	 *
	 * Replaces each mutable global variable's eager initializer with the type's default initializer
	 * and records a ScriptMainGlobalInit carrying the real initializer, so it runs from the
	 * synthetic `main` in source order instead of eagerly before it. Compile-time constants and
	 * immutable globals are left untouched: they cannot be reassigned, so the
	 * default-init-then-assign trick does not apply to them.
	 *
	 * @param ctx Active query context, used to build default initializer expressions.
	 * @param module_hout Definition module HOUT to neutralize; not mutated.
	 * @return Neutralized unit, the deferred initializers, and their backing storage.
	 */
	NeutralizedScriptModule neutralizeScriptGlobalInits(
		query::Context& ctx, const helios::HOUTUnit& module_hout
	);

	/**
	 * @brief Resolve the canonical root scope used for generated script `main`.
	 *
	 * Script compilation creates a chain of ephemeral REPL modules, one per statement,
	 * where each next module points to the previous one as REPL parent. The last module
	 * in that chain represents the full accumulated script context.
	 *
	 * Even though these modules are ephemeral, each of them still has a real main source file:
	 * createEphemeralChainedStatementModule() assigns one via createRandomVirtualFile(input).
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
	 * @param actions Ordered actions (wrapper calls and global-variable inits) to run from `main`,
	 *        in source order.
	 */
	helios::HOUTFunction buildScriptMainWrapper(
		query::Context&                      ctx,
		base::StrID                          script_id,
		helios::ScopeID                      main_scope,
		const std::vector<ScriptMainAction>& actions
	);

}  // namespace compiler::repl
