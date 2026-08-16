#include "coercions.hpp"

#include <ctv/numeric_value.hpp>
#include <helios/attributes/builtins.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/tsh/queries/implicit_coercibility.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/placeholder.hpp>
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

		// Coerces a single tuple element to the i-th component of the target tuple type.
		Box<code::Expr> coerceTupleElement(
			query::Context&               ctx,
			Box<code::Expr>               element,
			const tsh::TupleAbstractType& to_type,
			usize                         index
		) {
			auto element_coercion
				= canCoerce(ctx, element->expression_type, to_type.getComponents()[index])
			          .valueOrThrow();
			CORE_ASSERT(
				element_coercion.isValid(), "Coercion should always be valid at this point."
			);

			return element_coercion.coerce(ctx, std::move(element));
		}

		// Performs element by element coercion.
		Box<code::Expr> handleTupleCoercion(
			query::Context& ctx, Box<code::Expr> expr, const tsh::SymbolType<>& to
		) {
			auto source_type = expr->expression_type.getType().as<tsh::TupleAbstractType>();
			auto to_type     = to.getType().as<tsh::TupleAbstractType>();

			// A tuple literal is coerced element by element in place. Reading the elements back out
			// of a materialised tuple would lose the shape of the element expressions, which some
			// coercions still need - lifting a value to a `type` for instance only works on the
			// original expression. It also avoids materialising the source tuple altogether.
			if (auto* tuple_literal = dynamic_cast<code::TupleExpr*>(expr.get())) {
				std::vector<Box<code::Expr>> literal_elements;
				literal_elements.reserve(tuple_literal->elements.size());

				for (usize i = 0; i < tuple_literal->elements.size(); i++)
					literal_elements.emplace_back(
						coerceTupleElement(ctx, std::move(tuple_literal->elements[i]), to_type, i)
					);

				return makeBox<code::TupleExpr>(
					ctx, expr->origin.generatedFrom(), std::move(literal_elements)
				);
			}

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

				elements.emplace_back(coerceTupleElement(ctx, std::move(element), to_type, i));
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

			const auto& alternatives = to.getType().as<tsh::VariantAbstractType>().getUnderlyingTypes();
			for (usize i = 0; i < alternatives.size(); i++)
				if (tsh::isRefKindCoercible(from.getRefKind(), alternatives[i].getRefKind())
				    && alternatives[i].getType() == from.getType())
					return std::pair{ i, alternatives[i] };

			return {};
		}
	}

	PassingMethod passingMethod(query::Context& ctx, const tsh::ExpressionType<>& value) {
		const tsh::SymbolType<> symbol_type = value.getSymbolType();

		// If the value is being moved already, we pass it by `ByteCopy`.
		if (value.getValueCategory().mustMove()) return PassingMethod::ByteCopy;

		switch (value.getValueCategory().getCategory()) {
		case tsh::PrimaryCategory::Temporary:
			// This is a purely an optimization,
			// which keeps the generated HOUT free of `implicit_move` on everything.
			// A trivially destructible one owns nothing to hand over,
			// and moving it out of would produce exactly the same code as copying its bytes.
			if (symbol_type.isTriviallyDestructible(ctx) and symbol_type.isTriviallyCopyable(ctx))
				return PassingMethod::ByteCopy;
			if (value.getValueCategory().isMovableFrom()) return PassingMethod::ImplicitMove;
			return PassingMethod::ExplicitCopyOrMove;
		case tsh::PrimaryCategory::Literal:
			return PassingMethod::ByteCopy;
		case tsh::PrimaryCategory::Local:
		case tsh::PrimaryCategory::Global:
		case tsh::PrimaryCategory::Dereferenced:
			if (symbol_type.isTriviallyCopyable(ctx)) return PassingMethod::ByteCopy;
			return symbol_type.isCopyable(ctx) ? PassingMethod::ExplicitCopyOrMove
			                                   : PassingMethod::NotCopyable;
		}
		CORE_UNREACHABLE();
	}

	Box<code::Expr> moveReturnedLocal(query::Context& ctx, Box<code::Expr> value) {
		const tsh::ExpressionType<> type = value->expression_type;

		if (type.getValueCategory().getCategory() == tsh::PrimaryCategory::Local) {
			// A projection of a local is not movable from, for example.
			if (not type.getValueCategory().isMovableFrom()) return value;
			auto origin = value->origin.generatedFrom();
			return makeBox<code::MoveExpr>(
				ctx, origin, std::move(value), code::MoveExpr::MoveKind::Implicit
			);
		}
		return value;
	}

	IncompatibleTypesError::IncompatibleTypesError(
		dia::StablePosition                 given_position,
		Box<InteractiveType>                actual_type,
		Box<InteractiveType>                expected_type,
		base::Optional<dia::StablePosition> coercion_expects_pos
	):
		  MessageWithCodeFragment(given_position) {
		addArgument<dia::InteractiveArgument>("given_type", std::move(actual_type));
		addArgument<dia::InteractiveArgument>("expected_type", std::move(expected_type));
		addPointerMessage("given", given_position);
		if_opt_some(coercion_expects_pos, expected_pos) {
			addPointerMessage("expected", expected_pos);
		}
	}

	NoMatchingExpectedTypeError::NoMatchingExpectedTypeError(
		dia::StablePosition given_position, Box<InteractiveType> actual_type
	):
		  MessageWithCodeFragment(given_position) {
		addArgument<dia::InteractiveArgument>("given_type", std::move(actual_type));
		addPointerMessage("given", given_position);
	}

	void NoMatchingExpectedTypeError::addExploreAcceptedType(
		std::string accepted_type, Box<dia::MessageBase> coercion_error
	) {
		auto id = dia::MessageBase::getUniqueID();
		this->addLinkedMessage(id, std::move(coercion_error));

		std::vector<Box<dia::Argument>> args;
		args.emplace_back(makeBox<dia::TextArgument>("accepted_type", std::move(accepted_type)));
		args.emplace_back(makeBox<dia::TextArgument>("message_id", id));
		this->addExploreLink("accepted_type", std::move(args));
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

	query::QResult<Coercion> canCoerce(
		query::Context& ctx, const tsh::ExpressionType<>& from, const tsh::SymbolType<>& to
	) {
		const tsh::SymbolType<> from_type = from.getSymbolType();

		// First check that the type is even coercible to provide a invalid coercion error first.
		const bool coercible
			= ctx.query<tsh::QueryImplicitCoercibilityOnSymbolType>({ from_type, to });
		if (!coercible)
			return Coercion::invalid(from_type, to, InvalidCoercionReason::IncompatibleTypes);

		// Wrapping into a variant copies the value into one alternative, so that alternative is
		// what the copy is analysed against.
		const tsh::SymbolType<> copy_target
			= variantAlternativeFor(from_type, to).map([](const auto& alt) { return alt.second; }
			  ).copyValueOr(to);

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

	base::Optional<Box<code::Expr>> coerceFromBox(
		query::Context&                     ctx,
		Box<code::Expr>                     expr,
		const tsh::SymbolType<>             expected_type,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos,
		CoercionErrorOverrides              error_overrides
	) {
		const auto coercion_qresult = canCoerce(ctx, expr->expression_type, expected_type);
		if (coercion_qresult.hasFailed()) return {};
		const Coercion& coercion_result = coercion_qresult.valueOrPanic();
		if (coercion_result.isValid()) return coercion_result.coerce(ctx, std::move(expr));

		logCoercionFailure(
			ctx, coercion_result, source_position, coercion_expects_pos, std::move(error_overrides)
		);
		return {};
	}

	Box<dia::MessageBase> getCoercionError(
		query::Context&                     ctx,
		const Coercion&                     failed,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos
	) {
		switch (failed.getInvalidReason()) {
		case InvalidCoercionReason::IncompatibleTypes:
			return makeBox<IncompatibleTypesError>(
				source_position,
				makeBox<InteractiveType>(ctx, failed.validated_from),
				makeBox<InteractiveType>(ctx, failed.to),
				coercion_expects_pos
			);
		case InvalidCoercionReason::TypeNotCopyable:
			return makeBox<dia::PlaceholderError>(
				base::strConcat(
					"Type `",
					copiedValueType(failed.validated_from, failed.to).toString(),
					"` cannot be copied."
				),
				source_position
			);
		case InvalidCoercionReason::RequiresExplicitCopyMove: {
			// Reading a value out of a `ref`/`box` copies the pointee. `move` out of ref/box isn't
			// possible, so suggesting it here is misleading.
			const bool reads_through
				= readsThroughReference(failed.validated_from.getRefKind(), failed.to.getRefKind());

			const std::string message
				= reads_through
			        ? base::strConcat(
						  "` out of `",
						  failed.validated_from.toString(),
						  "`. Use `copy` to copy it out, `move` cannot move a value out of a "
						  "reference."
					  )
			        : std::string("`. Use `copy` to copy it or `move` to move it.");

			return makeBox<dia::PlaceholderError>(
				base::strConcat(
					"Cannot implicitly copy a value of non-trivially-copyable type `",
					copiedValueType(failed.validated_from, failed.to).toString(),
					message
				),
				source_position
			);
		}

		default:
			CORE_UNREACHABLE();
		}
	}

	void logCoercionFailure(
		query::Context&                     ctx,
		const Coercion&                     failed,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos,
		CoercionErrorOverrides              error_overrides
	) {
		// Use the caller's override if one is set, otherwise the default message.
		const base::Optional<CoercionErrorOverrides::Logger>& override = [&]() -> const auto& {
			switch (failed.getInvalidReason()) {
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
		ctx.logInt(getCoercionError(ctx, failed, source_position, coercion_expects_pos));
	}

	void logNoMatchingExpectedTypeFailure(
		query::Context&              ctx,
		const std::vector<Coercion>& failed_coercions,
		dia::StablePosition          source_position
	) {
		CORE_ASSERT(not failed_coercions.empty(), "Called with empty failed coercions.");

		auto error = makeBox<NoMatchingExpectedTypeError>(
			source_position, makeBox<InteractiveType>(ctx, failed_coercions.at(0).validated_from)
		);

		// Every accepted type was tried, so each of them gets an explore link pointing to the error
		// explaining why the coercion to it failed.
		for (auto& coercion: failed_coercions)
			error->addExploreAcceptedType(
				coercion.to.toString(), getCoercionError(ctx, coercion, source_position)
			);

		ctx.logInt(std::move(error));
	}
}
