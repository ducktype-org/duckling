#pragma once

#include "elements.hpp"

#include <base/visitor.hpp>

namespace compiler::helios::code {
	MAKE_VISITOR(HoutStmt,
		ReturnStmt,
		VoidReturnStmt,
		ExprStmt,
		IfStmt,
		VariableStmt,
		AssignmentStmt
	);
	MAKE_VISITOR(HoutExpr,
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
