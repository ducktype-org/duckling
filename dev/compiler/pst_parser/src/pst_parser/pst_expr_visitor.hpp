#pragma once

#include <base/exceptions.hpp>
#include <base/visitor.hpp>

#include "access.hpp"
#include "elements/elements_list.hpp"

namespace pst::expr {
	MAKE_ACCESS_VISITOR(
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
