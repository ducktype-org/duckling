// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "coercions.hpp"

#include "errors.hpp"
#include "passing.hpp"

#include <ctv/numeric_value.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/tsh/coercions/reference_coercion.hpp>
#include <helios/tsh/queries/implicit_coercibility.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::helios {
	namespace {
		using tsh::ExpressionType;

		Box<code::Expr> handleReferenceKindCoercion(
			query::Context& ctx, Box<code::Expr> expr, const tsh::SymbolType<>& to
		) {
			using rk       = tsh::ReferenceKind;
			auto origin    = expr->origin.generatedFrom();
			auto from_kind = expr->expression_type.getSymbolType().getRefKind();
			auto to_kind   = to.getRefKind();

			const bool is_pointer_like = from_kind == rk::Ref or from_kind == rk::Box;

			// Coercions to the the same reference kind are always allowed.
			if (from_kind == to_kind) return std::move(expr);
			// We also accept implicit auto deref from `ref` and `box`.
			else if (is_pointer_like && to_kind == rk::Direct) {
				// var x: T = ref_T;
				// var x: T = box_T;
				return makeBox<code::DerefExpr>(ctx, origin, std::move(expr));
			}
			// All the others shoule be explicit via `new` or `&`.
			CORE_PANIC(base::strConcat(
				"Illegal coercion, from: '",
				expr->expression_type.getSymbolType().toString(),
				"' to '",
				to.toString(),
				"' should be caught earlier"
			));
		}

		/**
		 * @brief Accesses each element of an expression that evaluates to a tuple.
		 */
		std::vector<Box<code::Expr>> accessTupleElements(query::Context& ctx, Box<code::Expr> expr) {
			auto source_type = expr->expression_type.getType().as<tsh::TupleAbstractType>();

			auto tuple  = makeBox<code::ReusableExpr>(ctx, std::move(expr));
			auto origin = tuple->origin.generatedFrom();

			std::vector<Box<code::Expr>> elements;
			elements.reserve(source_type.getComponents().size());

			for (usize i = 0; i < source_type.getComponents().size(); i++) {
				Box<code::Expr> base = tuple->nextUse();
				if (i == 0) base = tuple->clone();
				elements.emplace_back(makeBox<code::AccessExpr>(
					ctx,
					origin,
					std::move(base),
					ctx.query<defgen::QueryGeneratedSymbol>(
						{ .name = base::StrID{ base::strConcat("_", i + 1) },
				          .generated_symbol_data
				          = defgen::Field{ .parent_type = source_type, .index = i } }
					)
				));
			}

			return elements;
		}

		/**
		 * @brief Performs tuple->tuple coercion element by element.
		 */
		Box<code::Expr> handleTupleToTupleCoercion(
			query::Context& ctx, Box<code::Expr> expr, const tsh::SymbolType<>& to
		) {
			auto to_type = to.getType().as<tsh::TupleAbstractType>();

			auto saved_origin   = expr->origin.generatedFrom();
			auto tuple_elements = accessTupleElements(ctx, std::move(expr));

			auto elements
				= std::views::zip(tuple_elements, to_type.getComponents())
			    | std::views::transform([&](auto pair) {
					  auto& [element, target_type] = pair;
					  auto element_coercion
						  = canCoerce(ctx, element->expression_type, target_type).valueOrThrow();
					  CORE_ASSERT(
						  element_coercion.isValid(),
						  "Coercion should always be valid at this point."
					  );
					  return element_coercion.coerce(ctx, std::move(element));
				  })
			    | std::ranges::to<std::vector>();

			return makeBox<code::TupleExpr>(ctx, saved_origin, std::move(elements));
		}

		/**
		 * @brief Handles lifting a value to a type, which is a special case of coercion. Only a
		 * unit or tuple can be lifted to a type.
		 * @note Lifting a unit results in a sequence of expression to evaluate and the resulting
		 * meta unit literal.
		 * @note Lifting a tuple is done by evaluating its components element by element and
		 * returning a meta tuple literal with the resulting types.
		 */
		Box<code::Expr> handleLiftToType(query::Context& ctx, Box<code::Expr> expr) {
			using namespace code::shorthands;
			Shorthand s{ ctx };

			auto saved_origin = expr->origin.generatedFrom();

			if (expr->expression_type.getType().getKind() == tsh::Kind::Unit) {
				return withOrigin(
					saved_origin, s.seq(std::move(expr), s.litType(tsh::getUnitType()))
				);
			}
			if (expr->expression_type.getType().getKind() == tsh::Kind::Tuple) {
				auto tuple_elements = accessTupleElements(ctx, std::move(expr));
				auto elements
					= std::views::transform(
						  tuple_elements,
						  [&](auto& element) -> tsh::SymbolType<> {
							  auto ctv = ctx.query<QueryEvaluateHOUTExpression>({ element.ref() })
					                         .valueOrThrow();

							  variant_match(ctv.getStorage()) {
								  variant_case(tsh::SymbolType<>, type) { return type; }
								  variant_default {
									  CORE_PANIC(
										  "When lifting a tuple to a type all components should "
										  "evaluate to a type."
									  );
								  }
							  }
							  CORE_UNREACHABLE();
						  }
					  )
				    | std::ranges::to<std::vector>();

				return withOrigin(
					saved_origin, s.litType(ctx.query<tsh::QueryTupleType>({ std::move(elements) }))
				);
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief The variant alternative a value of type @p from is wrapped into, with its index.
		 *
		 * Wrapping a value into a variant copies it into the chosen alternative, so it is that
		 * alternative - and not the variant itself - that decides how the value is passed. A
		 * variant may not list one underlying type twice, so at most one alternative matches.
		 */
		base::Optional<std::pair<usize, tsh::SymbolType<>>> variantAlternativeFor(
			const tsh::SymbolType<>& from, const tsh::SymbolType<>& to
		) {
			if (to.getType().getKind() != tsh::Kind::Variant) return {};
			if (from.getType().getKind() == tsh::Kind::Variant) return {};

			const auto alternatives
				= to.getType().as<tsh::VariantAbstractType>().getUnderlyingTypes();
			for (usize i = 0; i < alternatives.size(); i++)
				if (tsh::referenceCoercionRule(from.getRefKind(), alternatives[i].getRefKind())
				        .isLegal()
				    && alternatives[i].getType() == from.getType())
					return std::pair{ i, alternatives[i] };

			return {};
		}
	}

	Box<code::Expr> Coercion::coerce(query::Context& ctx, Box<code::Expr> from) const {
		CORE_ASSERT(isValidFor(from.ref()), "Invalid expression for this coercion.");

		// An owned rvalue passed to a new owner is implicitly moved.
		if (transfers_ownership)
			from = makeBox<code::MoveExpr>(
				ctx, from->origin.generatedFrom(), std::move(from), code::MoveExpr::MoveKind::Implicit
			);

		// Wrapping into a variant happens before the reference kind is adjusted, as the chosen
		// alternative (and not the variant itself) decides whether the value is dereferenced.
		if (const auto alternative
		    = variantAlternativeFor(from->expression_type.getSymbolType(), to)) {
			const auto& [index, alternative_type] = alternative.value();

			auto alternative_expr
				= handleReferenceKindCoercion(ctx, std::move(from), alternative_type);
			return makeBox<code::VariantConstructExpr>(
				ctx, alternative_expr->origin.generatedFrom(), std::move(alternative_expr), to, index
			);
		}

		auto current_expr       = handleReferenceKindCoercion(ctx, std::move(from), to);
		auto source_symbol_type = current_expr->expression_type.getSymbolType();

		auto source_type = source_symbol_type.getType();

		bool is_source_numeric = source_type.getKind() == tsh::Kind::Integral
		                      or source_type.getKind() == tsh::Kind::Float;
		bool is_target_numeric = to.getType().getKind() == tsh::Kind::Integral
		                      or to.getType().getKind() == tsh::Kind::Float;
		bool is_source_bool = source_type.getKind() == tsh::Kind::Bool;
		bool is_target_bool = to.getType().getKind() == tsh::Kind::Bool;

		auto generated_origin = current_expr->origin.generatedFrom();
		if (source_type == to.getType()) {
			// No coercion
			return current_expr;
		} else if (source_type.getKind() == tsh::Kind::Void) {
			// We have to wrap the expression in CastExpr to change the inner type,
			// the cast itself is lowered to no-op.
			return makeBox<code::CastExpr>(ctx, generated_origin, std::move(current_expr), to);
		} else if ((is_source_numeric and is_target_numeric)
		           or (is_source_bool and is_target_numeric)) {
			// Numeric type promotion
			return makeBox<code::CastExpr>(ctx, generated_origin, std::move(current_expr), to);
		} else if (is_source_numeric and is_target_bool) {
			// Numeric zero-check to bool
			auto comparison = makeBox<code::BinaryOperatorExpr>(
				ctx,
				generated_origin,
				code::BuiltinBinary::IntegerNeq,
				std::move(current_expr),
				makeBox<code::LiteralNumericExpr>(
					ctx,
					code::generatedOrigin(),
					numeric_value::NumericValue::createOfType(source_type)
						.expect("Failed to create a NumericLiteral with 0 value. "
			                    "This should never happen.")
				)
			);
			return comparison;
		} else if ((source_type.getKind() == tsh::Kind::Unit
		            or source_type.getKind() == tsh::Kind::Tuple)
		           and to.getType().getKind() == tsh::Kind::Meta) {
			// Lift value to type
			return handleLiftToType(ctx, std::move(current_expr));
		} else if (source_type.getKind() == tsh::Kind::Tuple
		           and to.getType().getKind() == tsh::Kind::Tuple) {
			return handleTupleToTupleCoercion(ctx, std::move(current_expr), to);
		} else {
			CORE_PANIC("Coercion should always be valid at this point.");
		}
	}

	query::QResult<Coercion> canCoerce(
		query::Context& ctx, const tsh::ExpressionType<>& from, const tsh::SymbolType<>& to
	) {
		const tsh::SymbolType<> from_type = from.getSymbolType();

		// First check that the type is even coercible to provide a invalid coercion error first.
		const bool coercible
			= ctx.query<tsh::QueryImplicitCoercibilityOnSymbolType>({ from_type, to });
		if (!coercible)
			return Coercion::invalid(from_type, to, InvalidCoercionReason::IncompatibleTypes);

		// A `void` value never comes into existence, so there is nothing to copy or hand over:
		// the coercion only reconciles the types and lowers to nothing at all.
		if (from_type.getType().getKind() == tsh::Kind::Void)
			return Coercion::valid(from_type, to, false);

		// Wrapping into a variant copies the value into one alternative, so that alternative is
		// what the copy is analysed against.
		const tsh::SymbolType<> copy_target = variantAlternativeFor(from_type, to)
		                                          .map([](const auto& alt) { return alt.second; })
		                                          .copyValueOr(to);

		// A coercion that only rebinds a reference never copies, so it is always fine.
		if (not requiresValueCopy(from_type.getRefKind(), copy_target.getRefKind()))
			return Coercion::valid(from_type, to, false);

		// Otherwise a value has to be copied.
		switch (passingMethod(ctx, valueBeingCopied(from, copy_target))) {
		case PassingMethod::ByteCopy:
			return Coercion::valid(from_type, to, false);
		case PassingMethod::ImplicitMove:
			return Coercion::valid(from_type, to, true);
		case PassingMethod::ExplicitCopyOrMove:
			return Coercion::invalid(from_type, to, InvalidCoercionReason::RequiresExplicitCopyMove);
		case PassingMethod::NotCopyable:
			return Coercion::invalid(from_type, to, InvalidCoercionReason::TypeNotCopyable);
		}
		CORE_UNREACHABLE();
	}

	query::QResult<Coercion> canCoerceToMeta(query::Context& ctx, const tsh::ExpressionType<>& from) {
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

	query::QResult<Box<code::Expr>> coerceFromBox(
		query::Context&                     ctx,
		Box<code::Expr>                     expr,
		const tsh::SymbolType<>             expected_type,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos,
		CoercionErrorOverrides              error_overrides
	) {
		UNPACK_QRESULT(auto coercion =, canCoerce(ctx, expr->expression_type, expected_type));
		if (coercion.isValid()) return coercion.coerce(ctx, std::move(expr));

		logCoercionFailure(
			ctx, coercion, source_position, coercion_expects_pos, std::move(error_overrides)
		);
		return query::Failed();
	}
}
