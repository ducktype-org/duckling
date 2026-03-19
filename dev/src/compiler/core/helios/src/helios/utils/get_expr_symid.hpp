#pragma once

#include <helios/hout/elements/expr.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <base/collections/optional.hpp>

namespace compiler::helios {
	/**
	 * If expression is an identifier expression, returns its symbol ID.
	 * otherwise returns an empty optional.
	 */
	base::Optional<SymID> getIdentifierExprSymID(CRef<code::Expr> expr);
}
