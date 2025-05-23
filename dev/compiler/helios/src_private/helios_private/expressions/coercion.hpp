#pragma once

#include <helios/helios_result.hpp>
#include <helios/hout/elements/expr.hpp>
#include <typesystem/higher/symbol_type.hpp>

namespace compiler::helios {
    struct InvalidCoercion final {    };

    /**
     * Wraps expression with appropriate coercion expression.
     * @p from - expression to be coerced.
     * @p to - type to coerce to.
     */
    errors::HResult<Box<code::Expr>, InvalidCoercion> coerceExpression(
        Box<code::Expr> from, 
        tsh::SymbolType<> to
    );
}
