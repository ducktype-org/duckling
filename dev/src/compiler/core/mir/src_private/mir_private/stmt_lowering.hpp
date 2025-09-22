#pragma once

#include "mir_builders.hpp"

#include <helios/hout/elements.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/visitors.hpp>
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

	/**
	 * @brief Visitor that implements actual logic of lowering statements.
	 * @note The result of the visitor is stored in out member.
	 */
	struct StmtBlockVisitor: public hc::HoutStmtVisitor {
		BlockBuilderRef continuation;

		FunctionBuilder& function;

		/**
		 * Scope of the parent.
		 */
		ScopeRef parent_scope;

		StmtBlockVisitor(
			BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef parent_scope
		);

		base::Optional<StmtLowerRes> out;

		void output(StmtLowerRes value);

		void visitReturnStmt(const hc::ReturnStmt& stmt) override;
		void visitVoidReturnStmt(const hc::VoidReturnStmt&) override;
		void visitExprStmt(const hc::ExprStmt& stmt) override;
		void visitIfStmt(const hc::IfStmt& stmt) override;
		void visitWhileStmt(const hc::WhileStmt& stmt) override;
		void visitVariableStmt(const hc::VariableStmt& stmt) override;
		void visitAssignmentStmt(const hc::AssignmentStmt& stmt) override;
	};
}
