#pragma once

#include "mir_builders.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <mir/mir_structure/mir_local_ref.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <vector>

namespace compiler::mir {
	namespace hc = helios::code;

	/**
	 * @brief Represents result of statement lowering, which is
	 * a BlockBuilderRef that is the beginning of the lowered statement.
	 */
	struct StmtLowerRes final {
		BlockBuilderRef begin;
	};

	struct ControlFlowTarget final {
		helios::code::ControlFlowKind kind;
		base::Optional<helios::SymID> id;

		// Destination after leaving the construct with `break`.
		BlockBuilderRef break_target;

		// Destination for `continue`: condition evaluation for if/while,
		// iteration step for for, or body entry for block/if const.
		BlockBuilderRef continue_target;
	};

	/* Enclosing control-flow targets, with the innermost target stored last. */
	using ControlFlowTargets = std::vector<ControlFlowTarget>;

	/**
	 * @brief Lowers statement.
	 *
	 * @param stmt
	 * @param continuation Block that should be executed after this statement.
	 * @param function Function that we are lowering this statement in.
	 * @param parent_scope Scope of the parent of this Statement.
	 * @param targets
	 * @return StmtLowerRes
	 */
	StmtLowerRes lowerStmt(
		const hc::Stmt&    stmt,
		BlockBuilderRef    continuation,
		FunctionBuilder&   function,
		ScopeRef           parent_scope,
		ControlFlowTargets targets
	);

	/**
	 * @brief Lowers code-block, by lowering all statements in the block.
	 *
	 * @param code_block
	 * @param continuation Block that should be executed after this code block.
	 * @param function Function that we are lowering this code block in.
	 * @param targets
	 * @return StmtLowerRes
	 */
	StmtLowerRes lowerCodeBlock(
		const hc::CodeBlock&      code_block,
		BlockBuilderRef           continuation,
		FunctionBuilder&          function,
		ScopeRef                  parent_scope,
		const ControlFlowTargets& targets
	);

	// @TODO: #1333 StmtExprBoolJmpVisitor for jumping code
}
