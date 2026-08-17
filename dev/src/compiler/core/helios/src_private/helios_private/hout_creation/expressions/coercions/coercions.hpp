/**
 * @file coercions.hpp
 * @brief Implicit coercions: deciding whether a value of one type may be handed over where another
 * type is expected, and building the expression that performs it.
 */
#pragma once

#include "errors.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <base/pointers/box_or_ref.hpp>

#include <diagnostic/message.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {
	/**
	 * @brief This struct represents a function that performs a coercion from one expression to
	 * another. It was added to make sure that the coercion is always valid (by calling
	 * `canCoerce` first), and it checks if the expression being coerced and desired type
	 * is the same as the one validated.
	 */
	class Coercion final {
	public:
		/**
		 * The symbol type that was validated to be coercible.
		 */
		const tsh::SymbolType<> validated_from;

		/**
		 * The target type of the coercion.
		 */
		const tsh::SymbolType<> to;  // target type

		base::Optional<InvalidCoercionReason> invalid_reason;

		/**
		 * Whether this coercion moves the source value to the new owner implicitly. This has to be
		 * wrapped by an implicit `MoveExpr`.
		 */
		const bool transfers_ownership;

		[[nodiscard]]
		constexpr bool isValid() const noexcept {
			return invalid_reason.empty();
		}

		[[nodiscard]]
		constexpr bool isInvalid() const {
			return !isValid();
		}

		/**
		 * Check if the provided expression has the same type as the one validated.
		 */
		[[nodiscard]] bool isValidFor(CRef<code::Expr> expr) const noexcept {
			return isValid() && expr->expression_type.getSymbolType() == validated_from;
		}

		[[nodiscard]] bool isEmptyCoercion() const noexcept {
			// The implicit `MoveExpr` still has to be inserted, even when the types match.
			if (transfers_ownership) return false;
			return validated_from == to
			    || validated_from.withMutability(tsh::Mutability::Immutable) == to;
		}

		[[nodiscard]]
		InvalidCoercionReason getInvalidReason() const {
			CORE_ASSERT(
				isInvalid(), "Attempting to get the invalid reason from a valid CoercionResult."
			);
			return invalid_reason.value();
		}

		/**
		 * Main function that creates a coerced expression from the old one.
		 */
		[[nodiscard]] Box<code::Expr> coerce(query::Context& ctx, Box<code::Expr> from) const;

		/**
		 * Same as `coerce` but accepts a reference to the expression instead of taking ownership.
		 * This is useful when we don't know if the coercion will actually need to modify the
		 * expression or not (e.g., in case of empty coercion), so we can avoid unnecessary cloning.
		 */
		[[nodiscard]] BoxOrCRef<code::Expr> coerceFromRef(
			query::Context& ctx, CRef<code::Expr> from
		) const;

		static Coercion emptyCoercion(tsh::SymbolType<> from_and_to) {
			return { from_and_to, from_and_to, {}, false };
		}

	private:
		Coercion(
			tsh::SymbolType<>                     validated_from,
			tsh::SymbolType<>                     to,
			base::Optional<InvalidCoercionReason> invalid,
			bool                                  transfers_ownership
		):
			  validated_from(validated_from),
			  to(to),
			  invalid_reason(invalid),
			  transfers_ownership(transfers_ownership) {}

		friend query::QResult<Coercion> canCoerce(
			query::Context& ctx, const tsh::ExpressionType<>& from, const tsh::SymbolType<>& to
		);

		static Coercion invalid(
			tsh::SymbolType<> validated_from, tsh::SymbolType<> to, InvalidCoercionReason invalid
		) {
			return { validated_from, to, invalid, false };
		}

		static Coercion valid(
			tsh::SymbolType<> validated_from, tsh::SymbolType<> to, bool transfers_ownership
		) {
			return { validated_from, to, {}, transfers_ownership };
		}

		friend query::QResult<Coercion> canCoerce(
			query::Context& ctx, const tsh::ExpressionType<>& from, const tsh::SymbolType<>& to
		);
	};

	/**
	 * @brief Checks if a coercion of value described by from `from` to `to` is possible and returns
	 * a function performing the coercion if it is.
	 */
	query::QResult<Coercion> canCoerce(
		query::Context& ctx, const tsh::ExpressionType<>& from, const tsh::SymbolType<>& to
	);

	/**
	 * @brief Checks if a coercion of the value described by `from` to the meta type is possible and
	 * returns a function performing the coercion if it is.
	 * @note This is a wrapper around `canCoerce` for the common case of coercing to the meta type.
	 */
	query::QResult<Coercion> canCoerceToMeta(query::Context& ctx, const tsh::ExpressionType<>& from);

	/**
	 * @brief Checks whether `expr` can be coerced to `expected_type`. Returns a coerced expression
	 * when the coercion is valid, logs an error when `expr` is not coercible to the given type.
	 * Logs errors via `error_overrides` if ones are provided.
	 *
	 * @note This is a convenience wrapper around `canCoerce` + `coercion.coerce()` for the common
	 * case of coercing expressions with a `Box<code::Expr>` in hand, which is usual when handling
	 * compiler generated code.
	 * @return The coerced expression or an empty optional on error.
	 */
	base::Optional<Box<code::Expr>> coerceFromBox(
		query::Context&                     ctx,
		Box<code::Expr>                     expr,
		const tsh::SymbolType<>             expected_type,
		dia::StablePosition                 source_position,
		base::Optional<dia::StablePosition> coercion_expects_pos = {},
		CoercionErrorOverrides              error_overrides      = {}
	);
}
