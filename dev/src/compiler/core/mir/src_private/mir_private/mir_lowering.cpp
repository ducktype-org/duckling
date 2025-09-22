/**
 * @file mir_lowering.cpp
 * @brief Implementation of lowering HOUT functions to MIR functions.
 * The creation of MIR is done "in reverse" that is from function end to its beginning.
 */

#include "mir_lowering.hpp"

#include "mir_builders.hpp"
#include "stmt_lowering.hpp"

#include <helios/hout/elements.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <ranges>

namespace compiler::mir {
	namespace hc = helios::code;

	StmtLowerRes lowerCodeBlock(
		const hc::CodeBlock& code_block,
		BlockBuilderRef      continuation,
		FunctionBuilder&     function,
		ScopeRef             parent_scope
	) {
		StmtLowerRes last_result{ continuation };
		for (auto& stmt: code_block.statements | std::views::reverse) {
			last_result  = lowerStmt(*stmt, continuation, function, parent_scope);
			continuation = last_result.begin;
		}
		return last_result;
	}

	// @TODO: StmtExprBoolJmpVisitor for jumping code
}
