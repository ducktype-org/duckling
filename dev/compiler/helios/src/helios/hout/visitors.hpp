#pragma once
#include "elements.hpp"

namespace compiler::helios::code {

// visitors:
#define HOUT_VISITOR_METHOD(type) virtual void visit##type(const type& val) = 0;

	/**
	 * HoutStmtVisitor is a simple base class for VisitorPattern in `hout::Stmt`s.
	 * It is used by calling `stmt.acceptVisitor(visitor)`.
	 */
	class HoutStmtVisitor {
	public:
		HOUT_VISITOR_METHOD(ReturnStmt);
		HOUT_VISITOR_METHOD(VoidReturnStmt);
		HOUT_VISITOR_METHOD(ExprStmt);
		HOUT_VISITOR_METHOD(IfStmt);
		HOUT_VISITOR_METHOD(VariableStmt);

		virtual ~HoutStmtVisitor() = default;
	};

	/**
	 * HoutExprVisitor is a simple base class for VisitorPattern in `hout::Expr`s.
	 * It is used by calling `expr.acceptVisitor(visitor)`.
	 */
	class HoutExprVisitor {
	public:
		HOUT_VISITOR_METHOD(LiteralValueExpr);
		HOUT_VISITOR_METHOD(IdentifierExpr);
		HOUT_VISITOR_METHOD(BinaryOperatorExpr);
		HOUT_VISITOR_METHOD(UnaryOperatorExpr);
		HOUT_VISITOR_METHOD(ParenthesisExpr);
		HOUT_VISITOR_METHOD(KeywordExpr);
		HOUT_VISITOR_METHOD(TupleConstructorExpr);
		HOUT_VISITOR_METHOD(VariantConstructorExpr);

		virtual ~HoutExprVisitor() = default;
	};

// visitors:
#define HOUT_VISITOR_METHOD_PANIC(type) \
	void visit##type(const type&) override { CORE_PANIC("Panicky HOUT visitor: visited" #type); }

#define HOUT_VISITOR_METHOD_EMPTY(type) \
	void visit##type(const type&) override {}

	class HoutStmtVisitorPanicky: public HoutStmtVisitor {
	public:
		HOUT_VISITOR_METHOD_PANIC(ReturnStmt);
		HOUT_VISITOR_METHOD_PANIC(VoidReturnStmt);
		HOUT_VISITOR_METHOD_PANIC(ExprStmt);
		HOUT_VISITOR_METHOD_PANIC(IfStmt);
		HOUT_VISITOR_METHOD_PANIC(VariableStmt);
	};

	class HoutExprVisitorPanicky: public HoutExprVisitor {
	public:
		HOUT_VISITOR_METHOD_PANIC(LiteralValueExpr);
		HOUT_VISITOR_METHOD_PANIC(IdentifierExpr);
		HOUT_VISITOR_METHOD_PANIC(BinaryOperatorExpr);
		HOUT_VISITOR_METHOD_PANIC(UnaryOperatorExpr);
		HOUT_VISITOR_METHOD_PANIC(ParenthesisExpr);
		HOUT_VISITOR_METHOD_PANIC(KeywordExpr);
		HOUT_VISITOR_METHOD_PANIC(TupleConstructorExpr);
		HOUT_VISITOR_METHOD_PANIC(VariantConstructorExpr);
	};

	class HoutExprVisitorEmpty: public HoutExprVisitor {
	public:
		HOUT_VISITOR_METHOD_EMPTY(LiteralValueExpr);
		HOUT_VISITOR_METHOD_EMPTY(IdentifierExpr);
		HOUT_VISITOR_METHOD_EMPTY(BinaryOperatorExpr);
		HOUT_VISITOR_METHOD_EMPTY(UnaryOperatorExpr);
		HOUT_VISITOR_METHOD_EMPTY(ParenthesisExpr);
		HOUT_VISITOR_METHOD_EMPTY(KeywordExpr);
		HOUT_VISITOR_METHOD_EMPTY(TupleConstructorExpr);
		HOUT_VISITOR_METHOD_EMPTY(VariantConstructorExpr);
	};
}
