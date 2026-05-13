#include "coercions.hpp"

#include <ctv/numeric_value.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/tsh/queries/implicit_coercibility.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::helios {
	namespace {
		Box<code::Expr> handleReferenceKindCoercion(
			query::Context& ctx, Box<code::Expr> expr, const tsh::SymbolType<>& to
		) {
			auto origin    = expr->origin.generatedFrom();
			auto from_kind = expr->expression_type.getSymbolType().getRefKind();
			auto to_kind   = to.getRefKind();

			if (from_kind == to_kind) return std::move(expr);

			if (from_kind == tsh::ReferenceKind::Direct) {
				// --- From Direct ---
				if (to_kind == tsh::ReferenceKind::Ref)
					// Should be explicit: var x: ref T = &T;
					CORE_PANIC("Illegal Direct -> Ref coercion, should be caught earlier");
				else if (to_kind == tsh::ReferenceKind::Box) {
					// var x: box T = T(); -> Implicit box creation.
					return makeBox<code::BoxOfExpr>(ctx, origin, std::move(expr));
				}
			} else if (from_kind == tsh::ReferenceKind::Ref) {
				// --- From Reference ---
				if (to_kind == tsh::ReferenceKind::Direct) {
					// var x: T = ref_T; -> Dereference the rhs.
					// @TODO: #2000 Call a copy constructor here in the future.
					return makeBox<code::DerefExpr>(ctx, origin, std::move(expr));
				} else if (to_kind == tsh::ReferenceKind::Box) {
					// var x: box T = ref_T; -> Creating a box from a ref, requires to perform a
					// copy of the inner ref value. Since we can't just take ownership from a
					// reference, thus we first dereference the rhs.
					// @TODO: #2000 Call a copy constructor here in the future.
					auto dereferenced = makeBox<code::DerefExpr>(ctx, origin, std::move(expr));
					return makeBox<code::BoxOfExpr>(ctx, origin, std::move(dereferenced));
				}
			} else if (from_kind == tsh::ReferenceKind::Box) {
				// --- From Box ---
				if (to_kind == tsh::ReferenceKind::Direct)
					// var x: T = box_T; -> Dereference the rhs.
					// @TODO: #2000 Call a copy constructor here in the future.
					return makeBox<code::DerefExpr>(ctx, origin, std::move(expr));
				else if (to_kind == tsh::ReferenceKind::Ref)
					// Should be explicit: var x: ref T = &box_T;
					CORE_PANIC("Illegal Box -> Ref coercion, should be caught earlier");
			}

			return std::move(expr);
		}

		// Performs element by element coercion.
		Box<code::Expr> handleTupleCoercion(
			query::Context& ctx, Box<code::Expr> expr, const tsh::SymbolType<>& to
		) {
			auto source_type = expr->expression_type.getType().as<tsh::TupleAbstractType>();
			auto to_type     = to.getType().as<tsh::TupleAbstractType>();

			auto tuple = makeBox<code::ReusableExpr>(ctx, std::move(expr));

			std::vector<Box<code::Expr>> elements;
			elements.reserve(source_type.getComponents().size());

			for (usize i = 0; i < source_type.getComponents().size(); i++) {
				// We create a tuple expression with the correct type, and then let the
				// TupleExpr coercion handler handle the coercion to the target tuple type.
				auto element = makeBox<code::AccessExpr>(
					ctx,
					tuple->origin.generatedFrom(),
					tuple->clone(),
					ctx.query<defgen::QueryGeneratedSymbol>(
						{ .name = base::StrID{ base::strConcat("_", i + 1) },
				          .generated_symbol_data
				          = defgen::GeneratedSymbolData{ defgen::GeneratedSymbolData::Field{
							  .parent_type = source_type, .index = i } } }
					)
				);

				const auto& source_element_type = source_type.getComponents()[i];
				const auto& to_element_type     = to_type.getComponents()[i];

				auto element_coercion
					= canCoerce(ctx, source_element_type, to_element_type).valueOrThrow();
				CORE_ASSERT(
					element_coercion.isValid(), "Coercion should always be valid at this point."
				);

				elements.emplace_back(element_coercion.getCoercion().coerce(ctx, element->clone()));
			}

			return makeBox<code::TupleExpr>(ctx, tuple->origin.generatedFrom(), std::move(elements));
		}
	}

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

		auto current_expr       = handleReferenceKindCoercion(ctx, std::move(from), to);
		auto source_symbol_type = current_expr->expression_type.getSymbolType();

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
			return makeBox<code::CastExpr>(
				ctx, current_expr->origin.generatedFrom(), std::move(current_expr), to
			);
		} else if (is_source_numeric and is_target_bool) {
			// Numeric zero-check to bool
			auto comparison = makeBox<code::BinaryOperatorExpr>(
				ctx,
				current_expr->origin.generatedFrom(),
				code::BuiltinBinary::IntegerNeq,
				std::move(current_expr),
				makeBox<code::LiteralNumericExpr>(
					ctx,
					code::generatedOrigin(),
					numeric_value::NumericValue::createOfType(current_expr->expression_type.getType(
															  ))
						.expect("Failed to create a NumericLiteral with 0 value. This should never "
			                    "happen.")
				)
			);
			return comparison;
		} else if ((source_type.getKind() == tsh::Kind::Unit
		            or source_type.getKind() == tsh::Kind::Tuple)
		           and to.getType().getKind() == tsh::Kind::Meta) {
			// Lift value to type
			return makeBox<code::LiftToTypeExpr>(
				ctx, current_expr->origin.generatedFrom(), std::move(current_expr)
			);
		} else if (source_type.getKind() == tsh::Kind::Tuple
		           and to.getType().getKind() == tsh::Kind::Tuple) {
			return handleTupleCoercion(ctx, std::move(current_expr), to);
		} else {
			CORE_PANIC("Coercion should always be valid at this point.");
		}
	}

	CoercionQResult canCoerce(
		query::Context&         ctx,
		const tsh::SymbolType<> from,
		const tsh::SymbolType<> to,
		bool                    bypass_trivial_copyability_check
	) {
		// First check that the type is even coercible to provide a invalid coercion error first.
		const bool coercible = ctx.query<tsh::QueryImplicitCoercibilityOnSymbolType>({ from, to });
		if (!coercible) return InvalidCoercion{};

		// Coercion from reference/box to value requires a copy.
		// For now we only support trivial copyability.
		// @TODO: #2000 Remove this check.
		auto requires_copy_coercion = [](tsh::ReferenceKind from_kind, tsh::ReferenceKind to_kind) {
			switch (from_kind) {
			case tsh::ReferenceKind::Direct:
				return to_kind == tsh::ReferenceKind::Box || to_kind == tsh::ReferenceKind::Direct;
			case tsh::ReferenceKind::Ref:
				return to_kind == tsh::ReferenceKind::Direct || to_kind == tsh::ReferenceKind::Box;
			case tsh::ReferenceKind::Box:
				return to_kind == tsh::ReferenceKind::Direct;
			default:
				return false;
			}
		}(from.getRefKind(), to.getRefKind());


		if (!bypass_trivial_copyability_check && requires_copy_coercion
		    && !from.getType().isTriviallyCopyable(ctx))
			return TypeNotTriviallyCopyable{};

		// If we got here, then the type is coercible and trivially copyable.
		return Coercion(from, to);
	}

	CoercionQResult canCoerceToMeta(query::Context& ctx, const tsh::SymbolType<> from) {
		return canCoerce(
			ctx,
			from,
			tsh::SymbolType<>{
				tsh::getMetaType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			}
		);
	}

	BoxOrCRef<code::Expr> Coercion::coerceFromRef(query::Context& ctx, CRef<code::Expr> from) const {
		CORE_ASSERT(isValidFor(from), "Invalid expression for this coercion.");
		if (isEmptyCoercion()) return from;

		Box<code::Expr> from_box = from->clone();
		return coerce(ctx, std::move(from_box));
	}
}
