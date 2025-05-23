#pragma once

#include <base/optional.hpp>
#include <helios/hout/elements/expr.hpp>
#include <typesystem/higher/symbol_type.hpp>

namespace compiler::helios {
    
    base::Optional<Box<code::Expr>> generateCoercions(Box<code::Expr> from, tsh::SymbolType<> to) {
        // this is for now a mock:
        if (from->expression_type.getSymbolType() == to) {
            return from;
        } else {
            return {};
        }
    }
}
