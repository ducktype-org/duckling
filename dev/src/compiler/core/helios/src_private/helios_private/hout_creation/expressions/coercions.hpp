#pragma once

#include <diagnostic_interactive/message.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>

#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/pointers/box_or_ref.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/query_result.hpp>

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief The specific reason a coercion cannot be performed.
 */
MAKE_STRINGIFYABLE_ENUM(compiler::helios, uint8_t, InvalidCoercionReason, 
		/// The source type is not coercible to the target type.
		IncompatibleTypes,
		/// The value's type is not copyable, but this coercion required a copy.
		TypeNotCopyable,
		/// The value is copyable but not trivially copyable. The implicit copy must be made explicit
		/// with `copy` or `move` keyword.
		RequiresExplicitCopyMove
);

namespace compiler::helios {
	/**
	 * @brief How a coercion may hand over a value.
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
	PassingMethod passingMethod(query::Context& ctx, const tsh::ExpressionType<>& value);

	/**
	 * @brief Wraps a value in an implicit move when a `return` is about to end the life of the
	 * owned local it returns.
	 */
	[[nodiscard]] Box<code::Expr> moveReturnedLocal(query::Context& ctx, Box<code::Expr> value);

	class IncompatibleTypesError: public dia_int::MessageWithCodeFragment {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "incompatible_types" };
		}

	public:
		IncompatibleTypesError(
			dia_int::StablePosition                 given_position,
			Box<InteractiveType>                    actual_type,
			Box<InteractiveType>                    expected_type,
			base::Optional<dia_int::StablePosition> coercion_expects_pos
		);
	};

	/**
	 * @brief Error logged when an expression cannot be coerced to any of the several accepted
	 * types. The error of every attempted coercion is attached to this message.
	 */
	class NoMatchingExpectedTypeError final: public dia_int::MessageWithCodeFragment {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "no_matching_expected_type" };
		}

	public:
		NoMatchingExpectedTypeError(
			dia_int::StablePosition given_position, Box<InteractiveType> actual_type
		);

		/**
		 * @brief Adds an explore link for a single accepted type, pointing to the error of the
		 * coercion that was attempted to this type.
		 */
		void addExploreAcceptedType(
			std::string accepted_type, Box<dia_int::MessageBase> coercion_error
		);
	};

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
	 * @brief Optional overrides of the default coercion failure errors.
	 */
	struct CoercionErrorOverrides final {
		using Logger = std::function<void(query::Context&)>;

		/// Override for `InvalidCoercionReason::IncompatibleTypes`.
		base::Optional<Logger> incompatible_types = {};
		/// Override for `InvalidCoercionReason::TypeNotCopyable`.
		base::Optional<Logger> type_not_copyable = {};
		/// Override for `InvalidCoercionReason::RequiresExplicitCopyMove`.
		base::Optional<Logger> requires_explicit_copy_move = {};
	};

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
		query::Context&                         ctx,
		Box<code::Expr>                         expr,
		const tsh::SymbolType<>                 expected_type,
		dia_int::StablePosition                 source_position,
		base::Optional<dia_int::StablePosition> coercion_expects_pos = {},
		CoercionErrorOverrides                  error_overrides      = {}
	);

	/**
	 * @brief Builds the default diagnostic message describing why a coercion failed, based on the
	 * `reason`.
	 */
	[[nodiscard]] Box<dia_int::MessageBase> getCoercionError(
		query::Context&                         ctx,
		const Coercion&                         failed,
		dia_int::StablePosition                 source_position,
		base::Optional<dia_int::StablePosition> coercion_expects_pos = {}
	);

	/**
	 * @brief Logs the coercion failure for `reason`. Uses the matching override in
	 * `error_overrides` if one is provided, otherwise logs the default message.
	 */
	void logCoercionFailure(
		query::Context&                         ctx,
		const Coercion&                         failed,
		dia_int::StablePosition                 source_position,
		base::Optional<dia_int::StablePosition> coercion_expects_pos,
		CoercionErrorOverrides                  error_overrides = {}
	);

	/**
	 * @brief Logs the failure of coercing `source_symbol_type` to any of the `expected_types`.
	 * The error of every attempted coercion is attached to the logged message.
	 * @param failure_reasons Reason of the failure for every type of `expected_types`, in the same
	 * order. Must have the same size as `expected_types`.
	 */
	void logNoMatchingExpectedTypeFailure(
		query::Context&              ctx,
		const std::vector<Coercion>& failed_coercions,
		dia_int::StablePosition      source_position
	);
}
