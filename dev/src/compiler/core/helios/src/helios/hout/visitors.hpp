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
		LiteralNumericExpr,
		LiteralBoolExpr,
		LiteralStringExpr,
		LiteralTypeExpr,
		IdentifierExpr,
		BinaryOperatorExpr,
		UnaryOperatorExpr,
		TernaryOperatorExpr,
		ChainComparisonExpr,
		ParenthesisExpr,
		TupleExpr,
		VariantTypeConstructorExpr,
		CallExpr,
		AccessExpr,
		SequenceExpr,
		BoxOfExpr,
		RefOfExpr,
		DerefExpr,
		CastExpr,
		LiftToTypeExpr
	);
}
