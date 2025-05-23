#pragma once

#include <base/optional.hpp>
#include <helios/hout/elements/expr.hpp>
#include <typesystem/higher/symbol_type.hpp>

namespace compiler::helios {
    /**
     * Wraps expression with appropriate coercion expression.
     * @p from - expression to be coerced.
     * @p to - type to coerce to.
     * If coercion is not possible, returns empty optional.
     */
    base::Optional<Box<code::Expr>> generateCoercions(Box<code::Expr> from, tsh::SymbolType<> to);
}
