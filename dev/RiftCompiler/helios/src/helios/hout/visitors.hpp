#pragma once
#include "elements.hpp"

namespace compiler::helios::code {

	// visitors:
	#define HOUT_VISITOR_METHOD(type) \
		virtual void visit##type(const type& val) = 0;

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
		HOUT_VISITOR_METHOD(ExprStmt);


		virtual ~HoutExprVisitor() = default;
	};

}
