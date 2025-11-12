#pragma once

#include "elements.hpp"

#include <base/extend_cpp/visitor.hpp>

namespace compiler::helios::code {
	MAKE_VISITOR(HoutStmt,
		ReturnStmt,
		VoidReturnStmt,
		ExprStmt,
		IfStmt,
		WhileStmt,
		VariableStmt,
		AssignmentStmt
	);
	MAKE_VISITOR(HoutExpr,
		LiteralUnitExpr,
		LiteralIntExpr,
		LiteralBoolExpr,
		LiteralStringExpr,
		LiteralTypeExpr,
		IdentifierExpr,
		BinaryOperatorExpr,
		UnaryOperatorExpr,
		TernaryOperatorExpr,
		ChainComparisonExpr,
		ParenthesisExpr,
		TupleTypeConstructorExpr,
		VariantTypeConstructorExpr,
		CallExpr,
		AccessExpr,
		SequenceExpr,
		CastExpr
	);
}
