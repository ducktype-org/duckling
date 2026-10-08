// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
		BlockExpr
	);
}
