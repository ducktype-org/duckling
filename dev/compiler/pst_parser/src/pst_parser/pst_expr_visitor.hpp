#pragma once

#include "elements/elements_list.hpp"

#include <base/exceptions.hpp>
#include <base/visitor.hpp>

namespace pst::expr {
	MAKE_VISITOR(
		PstExpr,
		PrefixOperator,
		SuffixOperator,
		BinaryOperator,
		ExprValue,
		TemplateSpecifier,
		IdentifierLiteral,
		KeywordLiteral,
		Access,
		Call,
		ChainExpr,
		RoundExpr,
		BlockExpr,
		ComparisonChain,
		Ternary,
		Comma,
		Assignment
	);
};
