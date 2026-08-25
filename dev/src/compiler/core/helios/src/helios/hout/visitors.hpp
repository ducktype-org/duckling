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
		AssignmentStmt,
		BlockStmt
	);
	MAKE_VISITOR(HoutExpr,
		LiteralUnitExpr,
		LiteralNumericExpr,
		LiteralBoolExpr,
		LiteralCharExpr,
		LiteralStringExpr,
		LiteralTypeExpr,
		IdentifierExpr,
		ReusableExpr,
		BinaryOperatorExpr,
		UnaryOperatorExpr,
		TernaryOperatorExpr,
		ChainComparisonExpr,
		TupleExpr,
		VariantTypeConstructorExpr,
		VariantConstructExpr,
		MatchExpr,
		CallExpr,
		AccessExpr,
		IndexExpr,
		SequenceExpr,
		MoveExpr,
		RefOfExpr,
		PtrOfExpr,
		DerefExpr,
		DefaultValueExpr,
		CreateAggregateExpr,
		CastExpr,
		LiftToTypeExpr,
		BlockExpr
	);
}
