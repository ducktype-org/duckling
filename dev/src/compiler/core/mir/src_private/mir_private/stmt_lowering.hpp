#pragma once

#include "mir_builders.hpp"

#include <helios/hout/elements/stmt.hpp>
#include <mir/mir_structure/mir_structure.hpp>

namespace compiler::mir {
	namespace hc = helios::code;

	/**
	 * @brief Represents result of statement lowering, which is
	 * a BlockBuilderRef that is the beginning of the lowered statement.
	 */
	struct StmtLowerRes final {
		BlockBuilderRef begin;
	};

	/**
	 * @brief Lowers statement.
	 *
	 * @param stmt
	 * @param continuation Block that should be executed after this statement.
	 * @param function Function that we are lowering this statement in.
	 * @param parent_scope Scope of the parent of this Statement.
	 * @return StmtLowerRes
	 */
	StmtLowerRes lowerStmt(
		const hc::Stmt&  stmt,
		BlockBuilderRef  continuation,
		FunctionBuilder& function,
		ScopeRef         parent_scope
	);
	
	// @TODO: StmtExprBoolJmpVisitor for jumping code
}
