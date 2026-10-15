// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/pst_parser/elements/hierarchy/declarations/for.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/scope_id.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::helios::desugaring {
	/**
	 * @brief All symbols created by the `for` statement.
	 */
	struct ForGeneratedSymbols final {
		SymID iterator;  ///< The iterator variable
		SymID index;     ///< Variable storing the current index of the loop.
		SymID length;    ///< Variable storing the length of the iterable.
	};

	/**
	 * @brief The kinds of values a `for` statement can iterate over.
	 */
	enum class ForIterableKind {
		StaticArray,  ///< `T[n]`, iterated by index.
		Range,        ///< The `TemporaryRange` language primitive, created by `n..m`.
		List,         ///< `List[T]` from `core.containers`, iterated through `length()`/`at()`.
	};

	/**
	 * @brief What a `for` statement iterates over.
	 */
	struct ForIterableInfo final {
		ForIterableKind   kind = ForIterableKind::StaticArray;
		tsh::SymbolType<> element_type;  ///< The default type of the iterator variable.
	};

	/**
	 * @brief Classifies the type of a `for` iterable. Returns an empty optional when the type
	 * can't be iterated over (yet). Never logs an error.
	 */
	base::Optional<ForIterableInfo> getForIterableInfo(
		query::Context& ctx, const tsh::SymbolType<>& iterable_type
	);

	/**
	 * @brief Returns a structure containing all symbols created by a `for` statement.
	 */
	ForGeneratedSymbols getForGeneratedSymbols(query::Context& ctx, pst::Access<pst::For> stmt);

	/**
	 * @brief Callback for processing the for's body.
	 */
	using BodyProcessor = std::function<code::CodeBlock(pst::AccessLocked<pst::CodeBlockOrStmt>)>;

	/**
	 * @brief Performs desugaring of a for loop. Logs an error on failure.
	 *
	 * For a simple for loop:
	 * ```
	 * var t_arr: T[2];
	 * for(t in t_arr) {
	 *     builtin_output_i64(t.a);
	 * }
	 * ```
	 *
	 * HOUT like this would be generated:
	 * ```
	 * { # <- This block is generated (it's not here for beauty reasons).
	 * 	   #'41' is the scope hash combined with the variable name for the symbols to not collide in
	 * 	   # nested loops
	 *
	 *     var __index41 : u64 = 0;
	 *     do [tmp](refof((Symbol t_arr (26))))
	 * 	   var __len41: const u64 = 2; # or `len dynamic_arr` in case of lists.
	 *     while ((Symbol __index41 (63)) < (Symbol __len41 (64))) {
	 *         var t : Class T = deref([reuse](refof((Symbol t_arr (26)))))[(Symbol __index41 (63))];
	 *         do (Symbol builtin_output_i64 (16))(cast[to=i64]((Symbol t (62)).a))
	 *         (Symbol __index41 (63)) = (Symbol __index41 (63)) + 1;
	 *     }
	 * }
	 * ```
	 *
	 * A range `for (i in n..m)` reads both bounds once and copies the index into the iterator:
	 * ```
	 * {
	 *     do [tmp](n..m)
	 *     var __index : i64 = [tmp].start;
	 *     var __len : const i64 = [reuse].end;
	 *     while (__index < __len) {
	 *         var i : i64 = __index;
	 *         <body>
	 *         __index = __index + 1;
	 *     }
	 * }
	 * ```
	 *
	 * A `List[T]` uses its `length()` method once and copies every element out of `at()`:
	 * ```
	 * {
	 *     var __index : i64 = 0;
	 *     do [tmp](refof(list))
	 *     var __len : const i64 = length([tmp]);
	 *     while (__index < __len) {
	 *         var x : T = copy(deref(at([reuse], __index)));
	 *         <body>
	 *         __index = __index + 1;
	 *     }
	 * }
	 * ```
	 *
	 * @return The created code block containing the expanded for loop on success and an empty
	 * optional on failure.
	 */
	base::Optional<code::BlockStmt> desugarFor(
		query::Context&       ctx,
		pst::Access<pst::For> stmt,
		base::Optional<SymID> control_flow_id,
		const BodyProcessor&  process_body
	);
}
