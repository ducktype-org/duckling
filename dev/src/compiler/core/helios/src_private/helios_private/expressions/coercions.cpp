#include "coercions.hpp"

#include <typesystem/higher/queries/implicit_coercibility.hpp>

#include <query_framework/context.hpp>
#include "helios/hout/elements/expr.hpp"

namespace compiler::helios {


	/**
	 * @brief Create a zero literal HOUT expression of the given symbol type.
	 */
	Box<code::Expr> createZeroLiteralOfType(query::Context& ctx, const tsh::SymbolType<> type) {
		auto zero_literal = makeBox<code::LiteralNumericExpr>(ctx, i64(0));
		if (zero_literal->expression_type.getType() == type.getType()) return zero_literal;

		auto coerced = makeBox<code::CastExpr>(ctx, std::move(zero_literal), type);
		return coerced;
	}

	Box<code::Expr> Coercion::coerce(query::Context& ctx, Box<code::Expr> from) const {
		CORE_ASSERT(isValidFor(from.ref()), "Invalid expression for this coercion.");

		auto source_type = from->expression_type.getSymbolType().getType();

		bool is_source_numeric = source_type.getKind() == tsh::Kind::Integral
		                      or source_type.getKind() == tsh::Kind::Float;
		bool is_target_numeric = to.getType().getKind() == tsh::Kind::Integral
		                      or to.getType().getKind() == tsh::Kind::Float;
		bool is_source_bool = source_type.getKind() == tsh::Kind::Bool;
		bool is_target_bool = to.getType().getKind() == tsh::Kind::Bool;

		if (source_type == to.getType()) {
			return from;
		} else if ((is_source_numeric and is_target_numeric)
		           or (is_source_bool and is_target_numeric)) {
			return makeBox<code::CastExpr>(ctx, std::move(from), to);
		} else if (is_source_numeric and is_target_bool) {
			auto comparison = makeBox<code::BinaryOperatorExpr>(
				ctx,
				code::BuiltinBinary::IntegerNeq,
				std::move(from),
				createZeroLiteralOfType(ctx, from->expression_type.getSymbolType())
			);

			return comparison;
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
