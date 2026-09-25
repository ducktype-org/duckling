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
		TemplateSpecifier,
		IdentifierLiteral,
		KeywordLiteral,
		Access,
		Call,
		ChainExpr,
		RoundExpr,
		UnitExpr,
		BlockExpr,
		ArrayLiteralExpr,
		MatchExpr,
		ComparisonChain,
		Ternary,
		Comma,
		Assignment
	);
}
