#pragma once

#include <helios/hout/elements/expr.hpp>
#include <query_framework/query_int.hpp>

#include <base/optional.hpp>

namespace compiler::helios {
	base::Optional<SymID> querySymIDOfExpr(query::Context& ctx, CRef<code::Expr> expr);
}
