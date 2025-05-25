#include "coercions.hpp"

namespace compiler::helios {
    
    errors::HResult<Box<code::Expr>, InvalidCoercion> coerceExpression(
        Box<code::Expr> from, 
        tsh::SymbolType<> to
    ) {
        // this implementation is a mock:

        auto expected = from->expression_type.getSymbolType().getType();
        if (expected == to.getType()) {
            return from;
        } else if (expected.getKind() == tsh::Kind::Integral
                   and to.getType().getKind() == tsh::Kind::Integral) {
            // for now we allow any integral-to-integral coercion
            // without any conversions.
            return from;
        }
        else {
            return errors::HError{ InvalidCoercion{} };
        }
    }
}
