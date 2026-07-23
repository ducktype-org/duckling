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
#include <helios_private/symbols/symbols.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::helios {
	namespace {
		using tsh::ExpressionType;

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
					return makeBoxAllocCall(ctx, origin, std::move(expr));
				}
			} else if (from_kind == tsh::ReferenceKind::Ref) {
				// --- From Reference ---
				if (to_kind == tsh::ReferenceKind::Direct) {
					// var x: T = ref_T; -> Dereference the rhs.
					return makeBox<code::DerefExpr>(ctx, origin, std::move(expr));
				} else if (to_kind == tsh::ReferenceKind::Box) {
					// var x: box T = ref_T; -> Creating a box from a ref, requires to perform a
					// copy of the inner ref value. Since we can't just take ownership from a
					// reference, thus we first dereference the rhs.
					auto dereferenced = makeBox<code::DerefExpr>(ctx, origin, std::move(expr));
					return makeBoxAllocCall(ctx, origin, std::move(dereferenced));
				}
			} else if (from_kind == tsh::ReferenceKind::Box) {
				// --- From Box ---
				if (to_kind == tsh::ReferenceKind::Direct)
					// var x: T = box_T; -> Dereference the rhs.
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
				          = defgen::Field{ .parent_type = source_type, .index = i } }
					)
				);

				const auto& to_element_type = to_type.getComponents()[i];

				auto element_coercion
					= canCoerce(ctx, element->expression_type, to_element_type).valueOrThrow();
				CORE_ASSERT(
					element_coercion.isValid(), "Coercion should always be valid at this point."
				);

				elements.emplace_back(element_coercion.getCoercion().coerce(ctx, element->clone()));
			}

			return makeBox<code::TupleExpr>(ctx, tuple->origin.generatedFrom(), std::move(elements));
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
		 * @brief The value a copying coercion actually copies.
		 *
		 * When the reference-kind coercion reads a value out of a `ref`/`box`, the value that gets
		 * copied is the dereferenced one, not the reference itself.
		 */
		tsh::ExpressionType<> valueBeingCopied(
			const tsh::ExpressionType<>& from, const tsh::SymbolType<>& to
		) {
			const tsh::ReferenceKind from_kind = from.getSymbolType().getRefKind();
			const tsh::ReferenceKind to_kind   = to.getRefKind();

			const bool reads_through_reference
				= (from_kind == tsh::ReferenceKind::Ref
			       && (to_kind == tsh::ReferenceKind::Direct || to_kind == tsh::ReferenceKind::Box))
			   || (from_kind == tsh::ReferenceKind::Box && to_kind == tsh::ReferenceKind::Direct);

			if (!reads_through_reference) return from;
			return { from.getSymbolType().getPointeeSymbolType(),
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
			const auto type    = value.getType();
			const bool trivial = type.isTriviallyCopyable(ctx);

			switch (value.getValueCategory().getCategory()) {
			case tsh::PrimaryCategory::Temporary:
				return trivial ? PassingMethod::ByteCopy : PassingMethod::ImplicitMove;
			case tsh::PrimaryCategory::Literal:
				return PassingMethod::ByteCopy;
			case tsh::PrimaryCategory::Local:
			case tsh::PrimaryCategory::Global:
			case tsh::PrimaryCategory::Dereferenced:
				if (trivial) return PassingMethod::ByteCopy;
				return type.isCopyable(ctx) ? PassingMethod::ExplicitCopyOrMove
				                            : PassingMethod::NotCopyable;
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

	Box<code::Expr> Coercion::coerce(query::Context& ctx, Box<code::Expr> from) const {
		CORE_ASSERT(isValidFor(from.ref()), "Invalid expression for this coercion.");

		auto current_expr = handleReferenceKindCoercion(ctx, std::move(from), to);
		auto source_symbol_type = current_expr->expression_type.getSymbolType();

		auto source_type = source_symbol_type.getType();

		bool is_source_numeric = source_type.getKind() == tsh::Kind::Integral
		                      or source_type.getKind() == tsh::Kind::Float;
		bool is_target_numeric = to.getType().getKind() == tsh::Kind::Integral
		                      or to.getType().getKind() == tsh::Kind::Float;
		bool is_source_bool    = source_type.getKind() == tsh::Kind::Bool;
		bool is_target_bool    = to.getType().getKind() == tsh::Kind::Bool;

		if (source_type == to.getType()) {
			// No coercion
			return current_expr;
		} else if ((is_source_numeric and is_target_numeric) or (is_source_bool and is_target_numeric)) {
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
					numeric_value::NumericValue::createOfType(current_expr->expression_type.getType())
						.expect(
							"Failed to create a NumericLiteral with 0 value. This should never "
							"happen."
						)
				)
			);
			return comparison;
		} else if (
			(source_type.getKind() == tsh::Kind::Unit or source_type.getKind() == tsh::Kind::Tuple)
			and to.getType().getKind() == tsh::Kind::Meta
		) {
			// Lift value to type
			return makeBox<code::LiftToTypeExpr>(
				ctx, current_expr->origin.generatedFrom(), std::move(current_expr)
			);
		} else if (
			source_type.getKind() == tsh::Kind::Tuple and to.getType().getKind() == tsh::Kind::Tuple
		) {
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

	Box<dia_int::MessageBase> makeDefaultCoercionErrorMessage(
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
					source_symbol_type.withReferenceKind(tsh::ReferenceKind::Direct).toString(),
					"` cannot be copied."
				),
				source_position
			);
		case InvalidCoercionReason::RequiresExplicitCopyMove:
			return makeBox<dia_int::PlaceholderError>(
				base::strConcat(
					"Cannot implicitly copy a value of non-trivially-copyable type `",
					source_symbol_type.withReferenceKind(tsh::ReferenceKind::Direct).toString(),
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
		ctx.logInt(makeDefaultCoercionErrorMessage(
			ctx, reason, source_symbol_type, expected_type, source_position
		));
	}
}
