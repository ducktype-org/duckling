#include "coercions.hpp"

#include <typesystem/higher/mutability.hpp>
#include <typesystem/higher/queries/implicit_coercibility.hpp>

#include <query_framework/context.hpp>

namespace compiler::helios {


	Box<code::Expr> Coercion::coerce(query::Context& ctx, Box<code::Expr> from) const {
		CORE_ASSERT(isValidFor(from.ref()), "Invalid expression for this coercion.");
		auto expected = from->expression_type.getSymbolType().getType();
		bool is_source_numeric
			= expected.getKind() == tsh::Kind::Integral or expected.getKind() == tsh::Kind::Float;
		bool is_target_numeric = to.getType().getKind() == tsh::Kind::Integral
		                      or to.getType().getKind() == tsh::Kind::Float;
		bool is_source_bool = expected.getKind() == tsh::Kind::Bool;
		bool is_target_bool = to.getType().getKind() == tsh::Kind::Bool;


		if (expected == to.getType()) {
			return from;
		} else if ((is_source_numeric and is_target_numeric)
		           or (is_source_bool and is_target_numeric)
		           or (is_source_numeric and is_target_bool)) {
			return makeBox<code::CastExpr>(ctx, std::move(from), to);
		} else {
			CORE_PANIC("Coercion should always be valid at this point.");
		}
	}

	query::QResult<Coercion, InvalidCoercion> canCoerce(
		query::Context& ctx, tsh::SymbolType<> from, tsh::SymbolType<> to
	) {
		if (ctx.query<tsh::QueryImplicitCoercibilityOnSymbolType>({ from, to }))
			return Coercion(from, to);
		return query::QError{ InvalidCoercion{} };
	}

}
