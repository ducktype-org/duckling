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

		virtual ~HoutStmtVisitor() = default;
	};

	/**
	 * HoutExprVisitor is a simple base class for VisitorPattern in `hout::Expr`s.
	 * It is used by calling `expr.acceptVisitor(visitor)`.
	 */
	class HoutExprVisitor {
	public:
		HOUT_VISITOR_METHOD(ConstIntExprMock);
		HOUT_VISITOR_METHOD(IdentifierExpresion);

		virtual ~HoutExprVisitor() = default;
	};

// visitors:
#define HOUT_VISITOR_PANIC_METHOD(type) \
	void visit##type(const type&) override { RIFT_PANIC("Panicky HOUT visitor: visited" #type); }

	class HoutStmtPanickyVisitor: public HoutStmtVisitor {
	public:
		HOUT_VISITOR_PANIC_METHOD(ReturnStmt);
		HOUT_VISITOR_PANIC_METHOD(VoidReturnStmt);
		HOUT_VISITOR_PANIC_METHOD(ExprStmt);
	};

	class HoutExprPanickyVisitor: public HoutExprVisitor {
	public:
		HOUT_VISITOR_METHOD(ConstIntExprMock);
		HOUT_VISITOR_METHOD(IdentifierExpresion);
	};
}
