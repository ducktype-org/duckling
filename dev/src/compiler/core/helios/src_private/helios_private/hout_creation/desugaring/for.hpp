#pragma once

#include <frontend/pst_parser/elements/hierarchy/declarations/for.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <helios/hout/elements/stmt.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::helios::desugaring {
	/**
	 * @brief Returns SymID of the user-declared iterator of a For stmt.
	 */
	SymID getForIteratorSymbol(query::Context& ctx, pst::Access<pst::For> stmt);

	using BodyProcessor = std::function<code::CodeBlock(pst::AccessLocked<pst::CodeBlockOrStmt>)>;

	/**
	 * @brief Performs desugaring of a for loop. Logs an error on failure.
	 *
	 * For a simple for loop:
	 * ```
	 * for (x: i64 in static_arr) {
	 *     builtin_output_i64(x);
	 * }
	 * ```
	 *
	 * HOUT like this would be generated:
	 * ```
	 * { # <- This block is generated (it's not here for beauty reasons).
	 *		var __collection : ref i64[2] = &static_arr;
	 * 		var __index : u64 = 0u64;
	 * 		var __len: const u64 = 2; # or `len static_arr` in case of lists.
	 * 		while (__index < __len) {
	 * 		    var x : i64 = __collection[__index];
	 * 		    builtin_output_i64(x);
	 * 		    __index = __index + 1;
	 * 		}
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
