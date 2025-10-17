#pragma once

#include "access.hpp"
#include "elements/elements_list.hpp"

#include <base/except/exceptions.hpp>
#include <base/misc/visitor.hpp>

namespace pst::expr {
	MAKE_ACCESS_VISITOR(
		PstExpr,
		PrefixOperator,
		SuffixOperator,
		BinaryOperator,
		ExprValue,
		ExprStrValue,
		ExprCharValue,
		TemplateSpecifier,
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
