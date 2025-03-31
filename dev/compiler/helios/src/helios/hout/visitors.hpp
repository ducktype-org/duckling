#pragma once
#include <base/visitor.hpp>

#include "elements.hpp"

namespace compiler::helios::code {
	MAKE_VISITOR(
		HoutStmt, ReturnStmt, VoidReturnStmt, ExprStmt, IfStmt, VariableStmt, AssignmentStmt
	);
	MAKE_VISITOR(
		HoutExpr,
		LiteralIntExpr,
		LiteralBoolExpr,
		LiteralTypeExpr,
		IdentifierExpr,
		BinaryOperatorExpr,
		UnaryOperatorExpr,
		ParenthesisExpr,
		TupleTypeConstructorExpr,
		VariantTypeConstructorExpr,
		LinkedIdentifierExpr,
		CallExpr
	);
}
