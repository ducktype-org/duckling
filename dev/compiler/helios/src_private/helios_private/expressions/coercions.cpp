#include "coercions.hpp"

namespace compiler::helios {
    
    errors::HResult<Box<code::Expr>, InvalidCoercion> coerceExpression(
        Box<code::Expr> from, 
        tsh::SymbolType<> to
    ) {
        // this is for now a mock:
        if (from->expression_type.getSymbolType() == to) {
            return from;
        } else {
            return {};
        }
    }
}
