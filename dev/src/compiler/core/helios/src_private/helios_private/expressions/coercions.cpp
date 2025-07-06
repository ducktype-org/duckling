#include "coercions.hpp"

namespace compiler::helios {

	query::QResult<Box<code::Expr>, InvalidCoercion> coerceExpression(
		Box<code::Expr> from, tsh::SymbolType<> to
	) {
		// this implementation is a mock:

		auto expected = from->expression_type.getSymbolType().getType();
		bool is_expected_numeric
			= expected.getKind() == tsh::Kind::Integral or expected.getKind() == tsh::Kind::Float;
		bool is_to_numeric = to.getType().getKind() == tsh::Kind::Integral
		                  or to.getType().getKind() == tsh::Kind::Float;


		if (expected == to.getType()) {
			return from;
		} else if (is_expected_numeric and is_to_numeric) {
			// for now we allow (as a mock) any numeric coercion
			// without any conversions.
			return from;
		} else {
			return query::QError{ InvalidCoercion{} };
		}
	}
}
