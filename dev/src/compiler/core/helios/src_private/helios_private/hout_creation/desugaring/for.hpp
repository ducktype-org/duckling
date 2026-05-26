#pragma once

#include <frontend/pst_parser/elements/hierarchy/declarations/for.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/scope_id.hpp>

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
	 * @return The created code block containing the expanded for loop on success and an empty
	 * optional on failure.
	 */
	base::Optional<code::BlockStmt> desugarFor(
		query::Context& ctx, pst::Access<pst::For> stmt, const BodyProcessor& process_body
	);
}
