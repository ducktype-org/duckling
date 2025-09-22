#pragma once

#include "expr_lowering.hpp"
#include "stmt_lowering.hpp"

#include "mir_builders.hpp"

#include <mir/mir_structure/mir_structure.hpp>

namespace compiler::mir {
	/**
	 * @brief Lowers code-block, by lowering all statements in the block.
	 *
	 * @param code_block
	 * @param continuation Block that should be executed after this code block.
	 * @param function Function that we are lowering this code block in.
	 * @return StmtLowerRes
	 */
	StmtLowerRes lowerCodeBlock(
		const hc::CodeBlock& code_block,
		BlockBuilderRef      continuation,
		FunctionBuilder&     function,
		ScopeRef             parent_scope
	);

	// @TODO: StmtExprBoolJmpVisitor for jumping code
}
