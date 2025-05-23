#include "coercions.hpp"

namespace compiler::helios {
    
    errors::HResult<Box<code::Expr>, InvalidCoercion> coerceExpression(
        Box<code::Expr> from, 
        tsh::SymbolType<> to
    ) {
        // this is for now a mock:
        auto expected = from->expression_type.getSymbolType().getType();
        if (expected == to.getType()) {
            return from;
        } else {
            return errors::HError{ InvalidCoercion{} };
        }
    }
}
