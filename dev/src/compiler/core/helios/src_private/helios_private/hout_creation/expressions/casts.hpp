#include "diagnostic_interactive/stable_position.hpp"
#include "helios/hout/elements/expr.hpp"
#include "helios/tsh/symbol_type.hpp"

namespace compiler::helios::code {
    Box<Expr> castAs(
        query::Context& ctx,
        Box<Expr> value,
        tsh::SymbolType<> as_type,
        pst::Access<pst::expr::BinaryOperator> stmt
    );
}