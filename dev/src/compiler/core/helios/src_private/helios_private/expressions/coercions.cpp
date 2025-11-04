#include "coercions.hpp"

#include <typesystem/higher/mutability.hpp>
#include <typesystem/higher/queries/implicit_coercibility.hpp>

#include <query_framework/context.hpp>

namespace compiler::helios {


	Box<code::Expr> Coercion::coerce(Box<code::Expr> from) const {
		CORE_ASSERT(isValidFor(from.ref()), "Invalid expression for this coercion.");

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
			CORE_PANIC("Coercion should always be valid at this point.");
		}
	}

	query::QResult<Coercion, InvalidCoercion> canCoerce(
		query::Context& ctx, tsh::SymbolType<> from, tsh::SymbolType<> to
	) {
		if (from == to) return Coercion(from, to);

		if (ctx.query<tsh::QueryImplicitCoercibilityOnSymbolType>({ from, to }))
			return Coercion(from, to);
		return query::QError{ InvalidCoercion{} };
	}

}
