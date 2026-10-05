// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "access.hpp"
#include "elements/elements_list.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/visitor.hpp>

namespace pst::expr {
	MAKE_ACCESS_VISITOR(
		PstExpr,
		PrefixOperator,
		SuffixOperator,
		BinaryOperator,
		ExprNumericValue,
		ExprStrValue,
		ExprCharValue,
		ExprFormatStrValue,
		IdentifierLiteral,
		KeywordLiteral,
		Access,
		Call,
		ChainExpr,
		RoundExpr,
		UnitExpr,
		BlockExpr,
		MatchExpr,
		ComparisonChain,
		Ternary,
		Comma,
		Assignment
	);
}
