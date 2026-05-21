#pragma once

#include <frontend/pst_parser/elements/hierarchy/declarations/for.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <helios/hout/elements/stmt.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::helios::desugaring {
	/**
	 * @brief Returns SymID of the uses-declared iterator of a For stmt.
	 * TODOP: Docs
	 */
	SymID getForIteratorSymbol(query::Context& ctx, pst::Access<pst::For> stmt);

	using BodyProcessor = std::function<code::CodeBlock(pst::AccessLocked<pst::CodeBlockOrStmt>)>;

	base::Optional<code::BlockStmt> desugarFor(
		query::Context& ctx, pst::Access<pst::For> stmt, const BodyProcessor& process_body
	);
}
