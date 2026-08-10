#include "coercions.hpp"

#include <ctv/numeric_value.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
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

			auto tuple = makeBox<code::ReusableExpr>(ctx, std::move(expr));

			std::vector<Box<code::Expr>> elements;
			elements.reserve(source_type.getComponents().size());

			for (usize i = 0; i < source_type.getComponents().size(); i++) {
				elements.emplace_back(makeBox<code::AccessExpr>(
					ctx,
					tuple->origin.generatedFrom(),
					tuple->nextUse(),
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
		 * @brief Performs element by element coercion.
		 */
		Box<code::Expr> handleTupleCoercion(
			query::Context& ctx, Box<code::Expr> expr, const tsh::SymbolType<>& to
		) {
			auto to_type = to.getType().as<tsh::TupleAbstractType>();

			auto saved_origin = expr->origin.generatedFrom();
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
					  return element_coercion.getCoercion().coerce(ctx, std::move(element));
				  })
			    | std::ranges::to<std::vector>();

			return makeBox<code::TupleExpr>(ctx, saved_origin, std::move(elements));
		}

		/**
		 * @brief Whether the reference-kind transition creates a new value (which may require a
		 * copy).
		 */
		bool requiresValueCopy(tsh::ReferenceKind from_kind, tsh::ReferenceKind to_kind) {
			switch (from_kind) {
			case tsh::ReferenceKind::Direct:
			case tsh::ReferenceKind::Ref:
			case tsh::ReferenceKind::Box:
				return to_kind == tsh::ReferenceKind::Direct || to_kind == tsh::ReferenceKind::Box;
			}
			return false;
		}

		/**
		 * @brief Whether the reference-kind coercion reads the copied value out of a `ref`/`box`,
		 * in which case what gets copied is the dereferenced value, not the reference itself.
		 */
		bool readsThroughReference(tsh::ReferenceKind from_kind, tsh::ReferenceKind to_kind) {
			return (from_kind == tsh::ReferenceKind::Ref
			        && (to_kind == tsh::ReferenceKind::Direct || to_kind == tsh::ReferenceKind::Box))
			    || (from_kind == tsh::ReferenceKind::Box && to_kind == tsh::ReferenceKind::Direct);
		}

		/**
		 * @brief The type of the value a copying coercion actually copies.
		 *
		 * A `var a: box T = box_T` copies the box itself, reading a `box T` into a `T` copies the
		 * pointee.
		 */
		tsh::SymbolType<> copiedValueType(
			const tsh::SymbolType<>& from, const tsh::SymbolType<>& to
		) {
			if (!readsThroughReference(from.getRefKind(), to.getRefKind())) return from;
			return from.getPointeeSymbolType();
		}

		/**
		 * @brief The value a copying coercion actually copies.
		 */
		tsh::ExpressionType<> valueBeingCopied(
			const tsh::ExpressionType<>& from, const tsh::SymbolType<>& to
		) {
			const tsh::SymbolType<> from_type = from.getSymbolType();

			if (!readsThroughReference(from_type.getRefKind(), to.getRefKind())) return from;
			return { from_type.getPointeeSymbolType(),
				     tsh::ValueCategory(tsh::PrimaryCategory::Dereferenced) };
		}

		/**
		 * @brief How a copying coercion may hand over a value.
		 */
		enum class PassingMethod {
			ByteCopy,            ///< Trivially copyable, copied by copying its bytes.
			ImplicitMove,        ///< Owned rvalue (a temporary), moved implicitly.
			ExplicitCopyOrMove,  ///< Non-trivial assignable value (lvalue), needs an explicit
			                     ///< `copy`/`move`.
			NotCopyable,  ///< Non-trivial assignable value (lvalue) value with no copy constructor.
		};

		/**
		 * @brief Decides how the given value may passed, based on its value category and the
		 * abilities of its type.
		 */
		PassingMethod passingMethod(query::Context& ctx, const tsh::ExpressionType<>& value) {
			const auto symbol_type = value.getSymbolType();
			const bool trivial     = symbol_type.isTriviallyCopyable(ctx);

			switch (value.getValueCategory().getCategory()) {
			case tsh::PrimaryCategory::Temporary:
				return trivial ? PassingMethod::ByteCopy : PassingMethod::ImplicitMove;
			case tsh::PrimaryCategory::Literal:
				return PassingMethod::ByteCopy;
			case tsh::PrimaryCategory::Local:
			case tsh::PrimaryCategory::Global:
			case tsh::PrimaryCategory::Dereferenced:
				if (trivial) return PassingMethod::ByteCopy;
				return symbol_type.isCopyable(ctx) ? PassingMethod::ExplicitCopyOrMove
				                                   : PassingMethod::NotCopyable;
			}
			CORE_UNREACHABLE();
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
					saved_origin,
					s.seq(std::move(expr), s.litType(tsh::getUnitType()))
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
					saved_origin,
					s.litType(ctx.query<tsh::QueryTupleType>({ std::move(elements) }))
				);
			}
			CORE_UNREACHABLE();
		}
	}

	IncompatibleTypesError::IncompatibleTypesError(
		dia_int::StablePosition source_position,
		Box<InteractiveType>    actual_type,
		Box<InteractiveType>    expected_type
	):
		  MessageWithCodeFragmentAndCause(source_position) {
		addArgument<dia_int::InteractiveArgument>("given_type", std::move(actual_type));
		addArgument<dia_int::InteractiveArgument>("expected_type", std::move(expected_type));
	}

	NoMatchingExpectedTypeError::NoMatchingExpectedTypeError(
		dia_int::StablePosition source_position, Box<InteractiveType> actual_type
	):
		  MessageWithCodeFragmentAndCause(source_position) {
		addArgument<dia_int::InteractiveArgument>("given_type", std::move(actual_type));
	}

	void NoMatchingExpectedTypeError::addExploreAcceptedType(
		std::string accepted_type, Box<dia_int::MessageBase> coercion_error
	) {
		auto id = dia_int::MessageBase::getUniqueID();
		this->addLinkedMessage(id, std::move(coercion_error));

		std::vector<Box<dia_int::Argument>> args;
		args.emplace_back(makeBox<dia_int::TextArgument>("accepted_type", std::move(accepted_type)));
		args.emplace_back(makeBox<dia_int::TextArgument>("message_id", id));
		this->addExploreLink("accepted_type", std::move(args));
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
			return handleLiftToType(ctx, std::move(current_expr));
		} else if (source_type.getKind() == tsh::Kind::Tuple
		           and to.getType().getKind() == tsh::Kind::Tuple) {
			return handleTupleCoercion(ctx, std::move(current_expr), to);
		} else {
			CORE_PANIC("Coercion should always be valid at this point.");
		}
	}

	CoercionQResult canCoerce(
		query::Context& ctx, const tsh::ExpressionType<>& from, const tsh::SymbolType<>& to
	) {
		const tsh::SymbolType<> from_type = from.getSymbolType();

		// First check that the type is even coercible to provide a invalid coercion error first.
		const bool coercible
			= ctx.query<tsh::QueryImplicitCoercibilityOnSymbolType>({ from_type, to });
		if (!coercible) return InvalidCoercion{ InvalidCoercionReason::IncompatibleTypes };

		// A coercion that only rebinds a reference never copies, so it is always fine.
		if (not requiresValueCopy(from_type.getRefKind(), to.getRefKind()))
			return Coercion(from_type, to);

		// Otherwise a value has to be copied.
		switch (passingMethod(ctx, valueBeingCopied(from, to))) {
		case PassingMethod::ByteCopy:
		case PassingMethod::ImplicitMove:
			return Coercion(from_type, to);
		case PassingMethod::ExplicitCopyOrMove:
			return InvalidCoercion{ InvalidCoercionReason::RequiresExplicitCopyMove };
		case PassingMethod::NotCopyable:
			return InvalidCoercion{ InvalidCoercionReason::TypeNotCopyable };
		}
		CORE_UNREACHABLE();
	}

	CoercionQResult canCoerceToMeta(query::Context& ctx, const tsh::ExpressionType<>& from) {
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

	base::Optional<Box<code::Expr>> coerceFromBox(
		query::Context&         ctx,
		Box<code::Expr>         expr,
		const tsh::SymbolType<> expected_type,
		dia_int::StablePosition source_position,
		CoercionErrorOverrides  error_overrides
	) {
		const tsh::SymbolType<> source_symbol_type = expr->expression_type.getSymbolType();
		const auto coercion_qresult = canCoerce(ctx, expr->expression_type, expected_type);
		if (coercion_qresult.hasFailed()) return {};

		const CoercionResult& coercion_result = coercion_qresult.valueOrThrow();
		if (coercion_result.isValid())
			return coercion_result.getCoercion().coerce(ctx, std::move(expr));

		logCoercionFailure(
			ctx,
			coercion_result.getInvalidReason(),
			source_symbol_type,
			expected_type,
			source_position,
			std::move(error_overrides)
		);
		return {};
	}

	Box<dia_int::MessageBase> getCoercionError(
		query::Context&          ctx,
		InvalidCoercionReason    reason,
		const tsh::SymbolType<>& source_symbol_type,
		const tsh::SymbolType<>& expected_type,
		dia_int::StablePosition  source_position
	) {
		switch (reason) {
		case InvalidCoercionReason::IncompatibleTypes:
			return makeBox<IncompatibleTypesError>(
				source_position,
				makeBox<InteractiveType>(ctx, source_symbol_type),
				makeBox<InteractiveType>(ctx, expected_type)
			);
		case InvalidCoercionReason::TypeNotCopyable:
			return makeBox<dia_int::PlaceholderError>(
				base::strConcat(
					"Type `",
					copiedValueType(source_symbol_type, expected_type).toString(),
					"` cannot be copied."
				),
				source_position
			);
		case InvalidCoercionReason::RequiresExplicitCopyMove:
			return makeBox<dia_int::PlaceholderError>(
				base::strConcat(
					"Cannot implicitly copy a value of non-trivially-copyable type `",
					copiedValueType(source_symbol_type, expected_type).toString(),
					"`. Use `copy` to copy it or `move` to move it."
				),
				source_position
			);

		default:
			CORE_UNREACHABLE();
		}
	}

	void logCoercionFailure(
		query::Context&          ctx,
		InvalidCoercionReason    reason,
		const tsh::SymbolType<>& source_symbol_type,
		const tsh::SymbolType<>& expected_type,
		dia_int::StablePosition  source_position,
		CoercionErrorOverrides   error_overrides
	) {
		// Use the caller's override if one is set, otherwise the default message.
		const base::Optional<CoercionErrorOverrides::Logger>& override = [&]() -> const auto& {
			switch (reason) {
			case InvalidCoercionReason::IncompatibleTypes:
				return error_overrides.incompatible_types;
			case InvalidCoercionReason::TypeNotCopyable:
				return error_overrides.type_not_copyable;
			case InvalidCoercionReason::RequiresExplicitCopyMove:
				return error_overrides.requires_explicit_copy_move;
			default:
				CORE_UNREACHABLE();
			}
		}();

		if (override.has_value()) {
			(*override)(ctx);
			return;
		}
		ctx.logInt(getCoercionError(ctx, reason, source_symbol_type, expected_type, source_position)
		);
	}

	void logNoMatchingExpectedTypeFailure(
		query::Context&                           ctx,
		const tsh::SymbolType<>&                  source_symbol_type,
		const std::vector<tsh::SymbolType<>>&     expected_types,
		const std::vector<InvalidCoercionReason>& failure_reasons,
		dia_int::StablePosition                   source_position
	) {
		CORE_ASSERT(
			expected_types.size() == failure_reasons.size(),
			"Every expected type needs its own coercion failure reason."
		);

		auto error = makeBox<NoMatchingExpectedTypeError>(
			source_position, makeBox<InteractiveType>(ctx, source_symbol_type)
		);

		// Every accepted type was tried, so each of them gets an explore link pointing to the error
		// explaining why the coercion to it failed.
		for (usize i = 0; i < expected_types.size(); i++)
			error->addExploreAcceptedType(
				expected_types[i].toString(),
				getCoercionError(
					ctx, failure_reasons[i], source_symbol_type, expected_types[i], source_position
				)
			);

		ctx.logInt(std::move(error));
	}
}
