#include "coercions.hpp"

#include <ctv/numeric_value.hpp>
#include <helios/hout/elements/expr.hpp>
#include <typesystem/higher/queries/implicit_coercibility.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <query_frameworkcontext/context.hpp>

namespace compiler::helios {
	IncompatibleTypesError::IncompatibleTypesError(
		dia::SourcePosition  source_position,
		Box<InteractiveType> actual_type,
		Box<InteractiveType> expected_type
	):
		  MessageWithCodeFragmentAndCause(source_position) {
		addArgument<dia_int::InteractiveArgument>("given_type", std::move(actual_type));
		addArgument<dia_int::InteractiveArgument>("expected_type", std::move(expected_type));
	}

	Box<code::Expr> Coercion::coerce(query::Context& ctx, Box<code::Expr> from) const {
		CORE_ASSERT(isValidFor(from.ref()), "Invalid expression for this coercion.");

		auto current_expr       = std::move(from);
		auto source_symbol_type = current_expr->expression_type.getSymbolType();

		// If we are coercing from a reference type (`ref T` or `box T`) to a direct
		// type (`U`), we must first dereference the source expression.
		if (source_symbol_type.getRefKind() != tsh::ReferenceKind::Direct
		    && to.getRefKind() == tsh::ReferenceKind::Direct) {
			current_expr       = makeBox<code::DerefExpr>(ctx, std::move(current_expr));
			source_symbol_type = current_expr->expression_type.getSymbolType();
			// If underlying types differ, proceed with the standard coercion.
		}

		auto source_type = source_symbol_type.getType();

		bool is_source_numeric = source_type.getKind() == tsh::Kind::Integral
		                      or source_type.getKind() == tsh::Kind::Float;
		bool is_target_numeric = to.getType().getKind() == tsh::Kind::Integral
		                      or to.getType().getKind() == tsh::Kind::Float;
		bool is_source_bool = source_type.getKind() == tsh::Kind::Bool;
		bool is_target_bool = to.getType().getKind() == tsh::Kind::Bool;

		if (source_type == to.getType()) {
			// No coercion
			return current_expr;
		} else if ((is_source_numeric and is_target_numeric)
		           or (is_source_bool and is_target_numeric)) {
			// Numeric type promotion
			return makeBox<code::CastExpr>(ctx, std::move(current_expr), to);
		} else if (is_source_numeric and is_target_bool) {
			// Numeric zero-check to bool
			auto comparison = makeBox<code::BinaryOperatorExpr>(
				ctx,
				code::BuiltinBinary::IntegerNeq,
				std::move(current_expr),
				makeBox<code::LiteralNumericExpr>(
					ctx,
					numeric_value::NumericValue::createOfType(
						current_expr->expression_type.getSymbolType()
					)
						.expect("Failed to create a NumericLiteral with 0 value. This should never "
			                    "happen.")
				)
			);
			return comparison;
		} else if ((source_type.getKind() == tsh::Kind::Unit
		            or source_type.getKind() == tsh::Kind::Tuple)
		           and to.getType().getKind() == tsh::Kind::Meta) {
			// Lift value to type
			return makeBox<code::LiftToTypeExpr>(ctx, std::move(current_expr));
		} else {
			CORE_PANIC("Coercion should always be valid at this point.");
		}
	}

	CoercionQResult canCoerce(
		query::Context& ctx, const tsh::SymbolType<> from, const tsh::SymbolType<> to
	) {
		if (ctx.query<tsh::QueryImplicitCoercibilityOnSymbolType>({ from, to }))
			return Coercion(from, to);
		return InvalidCoercion{};
	}

	CoercionQResult canCoerceToMeta(query::Context& ctx, const tsh::SymbolType<> from) {
		return canCoerce(
			ctx,
			from,
			tsh::SymbolType<>{
				ctx.query<tsh::QueryMetaType>({}),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			}
		);
	}

}
