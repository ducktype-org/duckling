#pragma once

#include <helios/hout/elements/expr.hpp>
#include <pst_parser/access.hpp>
#include <pst_parser/elements/elements_list.hpp>
#include <query_framework/query_int.hpp>

#include <base/optional.hpp>

namespace compiler::helios {
	/**
	 * If expression is an identifier expression, returns its symbol ID.
	 * otherwise returns an empty optional.
	 */
	base::Optional<SymID> getIdentifierExprSymID(CRef<code::Expr> expr);
}
