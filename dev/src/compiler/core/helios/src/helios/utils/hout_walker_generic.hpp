#pragma once

#include <helios/hout/elements.hpp>
#include <helios/hout/hout.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <vector>

/**
 * @file hout_walker_generic.hpp
 * @brief Generic DFS traversal of the HOUT statement/expression tree.
 *
 * The walkers descend through every statement and into every sub-expression,
 * invoking a user-provided `Handler` for each expression they encounter.
 *
 * `Handler` is any callable that has templated operator (),
 * accepting the Expression types, like for example:
 * ```
 * template<typename T> operator()(const T& expr) {
 * if constexpr (std::same_as<std::remove_cvref_t<decltype(expr)>, code::CallExpr>) { ... }
 * }
 * ```
 *
 * See examples in the @hout_walkers.cpp file.
 */

namespace compiler::helios::code {
	/**
	 * @brief DFS walker over the HOUT tree. Hands every statement and every
	 * expression (in that order) to `handler`, then descends into the children.
	 *
	 * Derives from the panicky visitor bases so a newly added node type fails loudly
	 * until it is handled here, instead of being silently skipped.
	 *
	 * @tparam Handler callable accepting any concrete `Stmt`/`Expr` type by const ref.
	 */
	template<typename Handler>
	class HoutTreeWalker final: public HoutExprVisitorPanicky, public HoutStmtVisitorPanicky {
	public:
		explicit HoutTreeWalker(Handler& handler): handler(handler) {}

		void walk(const Expr& expr) { expr.acceptVisitor(*this); }

		void walk(const Stmt& stmt) { stmt.acceptVisitor(*this); }

		void walkBlock(const CodeBlock& block) {
			for (const auto& stmt: block.statements) walk(*stmt);
		}

	private:
		Handler& handler;

		// --- Statements ---

		void visitReturnStmt(const ReturnStmt& s) override {
			handler(s);
			walk(*s.value);
		}

		void visitVoidReturnStmt(const VoidReturnStmt& s) override { handler(s); }

		void visitExprStmt(const ExprStmt& s) override {
			handler(s);
			walk(*s.expr);
		}

		void visitVariableStmt(const VariableStmt& s) override {
			handler(s);
			walk(*s.initial_value);
		}

		void visitAssignmentStmt(const AssignmentStmt& s) override {
			handler(s);
			walk(*s.location_expr);
			walk(*s.new_value_expr);
		}

		void visitIfStmt(const IfStmt& s) override {
			handler(s);
			walk(*s.condition);
			walkBlock(s.then_body);
			walkBlock(s.else_body);
		}

		void visitWhileStmt(const WhileStmt& s) override {
			handler(s);
			walk(*s.condition);
			walkBlock(s.body);
		}

		void visitBlockStmt(const BlockStmt& s) override {
			handler(s);
			walkBlock(s.body);
		}

		// --- Leaf expressions: hand to the handler, nothing to descend into. ---

		void visitLiteralUnitExpr(const LiteralUnitExpr& e) override { handler(e); }

		void visitLiteralNumericExpr(const LiteralNumericExpr& e) override { handler(e); }

		void visitLiteralBoolExpr(const LiteralBoolExpr& e) override { handler(e); }

		void visitLiteralCharExpr(const LiteralCharExpr& e) override { handler(e); }

		void visitLiteralStringExpr(const LiteralStringExpr& e) override { handler(e); }

		void visitLiteralTypeExpr(const LiteralTypeExpr& e) override { handler(e); }

		void visitIdentifierExpr(const IdentifierExpr& e) override { handler(e); }

		void visitDefaultValueExpr(const DefaultValueExpr& e) override { handler(e); }

		// --- Composite expressions: hand to the handler (pre-order), then descend. ---

		void visitReusableExpr(const ReusableExpr& e) override {
			handler(e);
			if (e.first_use) walk(*e.inner);
		}

		void visitBinaryOperatorExpr(const BinaryOperatorExpr& e) override {
			handler(e);
			walk(*e.lhs);
			walk(*e.rhs);
		}

		void visitUnaryOperatorExpr(const UnaryOperatorExpr& e) override {
			handler(e);
			walk(*e.expr);
		}

		void visitTernaryOperatorExpr(const TernaryOperatorExpr& e) override {
			handler(e);
			walk(*e.condition);
			walk(*e.if_true);
			walk(*e.if_false);
		}

		void visitChainComparisonExpr(const ChainComparisonExpr& e) override {
			handler(e);
			for (const auto& comparison: e.comparisons) walk(*comparison);
		}

		void visitParenthesisExpr(const ParenthesisExpr& e) override {
			handler(e);
			walk(*e.inner);
		}

		void visitTupleExpr(const TupleExpr& e) override {
			handler(e);
			for (const auto& element: e.elements) walk(*element);
		}

		void visitVariantTypeConstructorExpr(const VariantTypeConstructorExpr& e) override {
			handler(e);
			for (const auto& subtype: e.subtypes) walk(*subtype);
		}

		void visitCallExpr(const CallExpr& e) override {
			handler(e);
			walk(*e.callee);
			for (const auto& argument: e.arguments) walk(*argument);
		}

		void visitAccessExpr(const AccessExpr& e) override {
			handler(e);
			walk(*e.base);
		}

		void visitIndexExpr(const IndexExpr& e) override {
			handler(e);
			walk(*e.base);
			walk(*e.index);
		}

		void visitSequenceExpr(const SequenceExpr& e) override {
			handler(e);
			for (const auto& expression: e.expressions) walk(*expression);
		}

		void visitRefOfExpr(const RefOfExpr& e) override {
			handler(e);
			walk(*e.inner);
		}

		void visitDerefExpr(const DerefExpr& e) override {
			handler(e);
			walk(*e.inner);
		}

		void visitCastExpr(const CastExpr& e) override {
			handler(e);
			walk(*e.source_expr);
		}

		void visitLiftToTypeExpr(const LiftToTypeExpr& e) override {
			handler(e);
			walk(*e.value_expr);
		}

		void visitBlockExpr(const BlockExpr& e) override {
			handler(e);
			walk(*e.block);
		}

		void visitListPushExpr(const ListPushExpr& e) override {
			handler(e);
			walk(*e.list);
			walk(*e.element);
		}

		void visitListPopExpr(const ListPopExpr& e) override {
			handler(e);
			walk(*e.list);
			walk(*e.count);
		}

		void visitMoveExpr(const MoveExpr& e) override {
			handler(e);
			walk(*e.inner);
		}
	};

	/// Walk `expr` and all of its sub-expressions.
	template<typename Handler>
	void walkExprTree(const Expr& expr, Handler handler) {
		HoutTreeWalker<Handler> walker(handler);
		walker.walk(expr);
	}

	/// Walk `stmt` and everything reachable from it (nested blocks included).
	template<typename Handler>
	void walkStmtTree(const Stmt& stmt, Handler handler) {
		HoutTreeWalker<Handler> walker(handler);
		walker.walk(stmt);
	}

	/// Walk every statement and expression reachable from a code block.
	template<typename Handler>
	void walkCodeBlock(const CodeBlock& block, Handler handler) {
		HoutTreeWalker<Handler> walker(handler);
		walker.walkBlock(block);
	}
}

namespace compiler::helios {
	/// Walk a function body (nested blocks included). No-op for bodyless declarations.
	template<typename Handler>
	void walkFunctionTree(const HOUTFunction& fun, Handler handler) {
		if (fun.body) code::walkCodeBlock(*fun.body, std::move(handler));
	}
}
