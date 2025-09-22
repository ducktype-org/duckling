#pragma once

#include "mir_builders.hpp"

#include <helios/hout/elements.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/hout/visitors.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <variant>

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
	 * @brief Represents a partial result of expression lowering.
	 *
	 * This consists of a BlockBuilderRef marking the beginning of the lowered
	 * expression and either:
	 *  - a MIRValue holding the result of the expression, OR
	 *  - a Finalizer representing the last instruction that saves the result
	 *    without specifying its target.
	 *
	 * For complete lowering, call the dedicated function that stores the result
	 * in the desired location while (if possible) avoiding the creation of unnecessary temporaries.
	 *
	 * In most cases, use getResult() or storeResultInGivenVariable().
	 */
	struct ExprLowerRes final {
		BlockBuilderRef begin;

		/**
		 * @brief Represents a finalizer instruction that saves the result of an expression.
		 * Stores hole where instruction will be saved, instruction without output and type of
		 * result. This instruction can be performed on provided varaible
		 * (storeResultInGivenVariable) or generated temporary (getResult).
		 */
		struct Finalizer final {
			BlockBuilder::InstructionHole hole;
			Instruction                   instr;
			tsh::SymbolType<>             type;
		};

		std::variant<MIRValue, Finalizer> value;

		ExprLowerRes(BlockBuilderRef begin, std::variant<MIRValue, Finalizer> value);

		/**
		 * @brief helper function returing type of result. Can be used if MIRValue is not stored.
		 */
		[[nodiscard]]
		tsh::SymbolType<> getResultType();

		/**
		 * @brief Helper function that returns MIRvalue if it is already stored in structure.
		 */
		[[nodiscard]]
		base::Optional<MIRValue> getResultIfStored();

		/**
		 * @brief If result of expr is value already returns it,
		 * Otherwise creates temporary, makes last instruction save res there and returns it.
		 * @note may use InstructionHole stored in structure, probably use only once.
		 */
		[[nodiscard]]
		MIRValue getResult(FunctionBuilder& function);

		/**
		 * @brief If result of expr is value it creates
		 * instruction that will assign result to it. Otherwise it makes the last instruction of the
		 * expression save result directly to the target.
		 * @note may use InstructionHole stored in stucture, probably use only once.
		 */
		void storeResultInGivenVariable(
			const std::variant<LocalRef, MirGlobal>& target,
			BlockBuilder::InstructionHole&           hole,
			const std::vector<OperationFlag>&        flags,
			ScopeRef                                 scope
		);
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
	 * @brief Lowers expression.
	 *
	 * @param expr
	 * @param continuation Block that should be executed after this expression.
	 * @param function Function that we are lowering this expression in.
	 * @param expr_scope Lifetime Scope this expression should be in.
	 * @return ExprLowerRes
	 */
	ExprLowerRes lowerExpr(
		const hc::Expr&  expr,
		BlockBuilderRef  continuation,
		FunctionBuilder& function,
		ScopeRef         expr_scope
	);

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

	/**
	 * @brief Visitor that implements actual logic of lowering expression.
	 * @note The result of the visitor is stored in out member. To store expr
	 * result somewhere, call finalize with place to store it
	 */
	struct ExprBlockVisitor final: public hc::HoutExprVisitor {
		BlockBuilderRef continuation;

		base::Optional<ExprLowerRes> out;

		FunctionBuilder& function;

		/**
		 * The scope of the expression, where it and its result should live in.
		 */
		ScopeRef expr_scope;

		ExprBlockVisitor(
			BlockBuilderRef continuation, FunctionBuilder& function, ScopeRef expr_scope
		);

		void output(ExprLowerRes&& lowering_result);

		void valueOutput(BlockBuilderRef begin, const MIRValue& value);

		void noValueOutput(
			BlockBuilderRef                      begin,
			const BlockBuilder::InstructionHole& hole,
			const Instruction&                   instr,
			const tsh::SymbolType<>&             type
		);

		void visitLiteralIntExpr(const hc::LiteralIntExpr&) override;
		void visitLiteralBoolExpr(const hc::LiteralBoolExpr&) override;
		void visitLiteralStringExpr(const hc::LiteralStringExpr&) override;
		void visitLiteralTypeExpr(const hc::LiteralTypeExpr&) override;
		void visitIdentifierExpr(const hc::IdentifierExpr&) override;
		void visitBinaryOperatorExpr(const hc::BinaryOperatorExpr&) override;
		void visitUnaryOperatorExpr(const hc::UnaryOperatorExpr&) override;
		void visitTernaryOperatorExpr(const helios::code::TernaryOperatorExpr&) override;
		void visitParenthesisExpr(const hc::ParenthesisExpr&) override;
		void visitTupleTypeConstructorExpr(const hc::TupleTypeConstructorExpr&) override;
		void visitVariantTypeConstructorExpr(const hc::VariantTypeConstructorExpr&) override;
		void visitAccessExpr(const hc::AccessExpr&) override;
		void visitSequenceExpr(const hc::SequenceExpr&) override;
		void visitChainComparisonExpr(const hc::ChainComparisonExpr&) override;
		void visitCallExpr(const hc::CallExpr&) override;

	private:
		static Operation builtinBinaryToOperation(const hc::BuiltinBinary builtin);
		static Operation builtinUnaryToOperation(const hc::BuiltinUnary builtin);

		/**
		 * Get the type of a location, assuming that it is a local value.
		 * @param location A MIR location which holds a local value.
		 * @param ctx The query context for AbstractType generation.
		 * @return The type of the local value.
		 */
		static tsh::SymbolType<> locationType(const MIRValue location, query::Context& ctx);
	};

	// @TODO: StmtExprBoolJmpVisitor for jumping code
}
