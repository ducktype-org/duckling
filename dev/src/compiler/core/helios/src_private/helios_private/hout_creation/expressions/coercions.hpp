#pragma once

#include <diagnostic_interactive/message.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>

#include <base/pointers/box_or_ref.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {


	class IncompatibleTypesError: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "incompatible_types" };
		}

	public:
		IncompatibleTypesError(
			dia_int::StablePosition source_position,
			Box<InteractiveType>    actual_type,
			Box<InteractiveType>    expected_type
		);
	};

	/**
	 * @brief Type used to indicate an invalid coercion, i.e. coercion that cannot be performed.
	 */
	struct InvalidCoercion final {};

	/**
	 * @brief Type used to indicate a copy of a non-trivially-copyable type.
	 */
	struct TypeNotTriviallyCopyable final {};

	class CoercionResult;
	using CoercionQResult = query::QResult<CoercionResult>;

	/**
	 * @brief This struct represents a function that performs a coercion from one expression to
	 * another. It was added to make sure that the coercion is always valid (by calling
	 * `canCoerce` first), and it checks if the expression being coerced and desired type
	 * is the same as the one validated.
	 */
	class Coercion final {
	public:
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

		/**
		 * The symbol type that was validated to be coercible.
		 */
		const tsh::SymbolType<> validated_from;

		/**
		 * The target type of the coercion.
		 */
		const tsh::SymbolType<> to;  // target type

		/**
		 * Check if the provided expression has the same type as the one validated.
		 */
		[[nodiscard]] bool isValidFor(CRef<code::Expr> expr) const noexcept {
			return expr->expression_type.getSymbolType() == validated_from;
		}

		friend CoercionQResult canCoerce(
			query::Context& ctx, tsh::SymbolType<> from, tsh::SymbolType<> to
		);

		[[nodiscard]] bool isEmptyCoercion() const noexcept {
			return validated_from == to
			    || validated_from.withMutability(tsh::Mutability::Immutable) == to;
		}

		static Coercion emptyCoercion(tsh::SymbolType<> from_and_to) {
			return { from_and_to, from_and_to };
		}

	private:
		Coercion(tsh::SymbolType<> validated_from, tsh::SymbolType<> to):
			  validated_from(validated_from),
			  to(to) {}
	};

	/**
	 * @brief The result of a coercion check, either a valid Coercion or an InvalidCoercion.
	 * This is mostly a utility wrapper around std::variant, that helps in avoiding boilerplate code.
	 */
	class CoercionResult final {
	public:
		CoercionResult(Coercion coercion): storage(std::move(coercion)) {}

		CoercionResult(InvalidCoercion invalid): storage(invalid) {}

		CoercionResult(TypeNotTriviallyCopyable invalid): storage(invalid) {}

		[[nodiscard]]
		constexpr bool isValid() const noexcept {
			return std::holds_alternative<Coercion>(storage);
		}

		[[nodiscard]]
		constexpr bool isInvalid() const {
			return !isValid();
		}

		[[nodiscard]]
		const Coercion& getCoercion() const& {
			CORE_ASSERT(isValid(), "Attempting to get Coercion from an invalid CoercionResult.");
			return std::get<Coercion>(storage);
		}

		[[nodiscard]]
		Coercion&& getCoercion() && {
			CORE_ASSERT(isValid(), "Attempting to get Coercion from an invalid CoercionResult.");
			return std::move(std::get<Coercion>(storage));
		}

		/**
		 * Main function that creates a coerced expression from the old one.
		 */
		[[nodiscard]] Box<code::Expr> coerce(query::Context& ctx, Box<code::Expr> from) const {
			CORE_ASSERT(isValid(), "Attempting to get Coercion from an invalid CoercionResult.");
			return std::get<Coercion>(storage).coerce(ctx, std::move(from));
		}

		[[nodiscard]]
		const std::variant<Coercion, InvalidCoercion, TypeNotTriviallyCopyable>& getVariant() const {
			return storage;
		}


	private:
		std::variant<Coercion, InvalidCoercion, TypeNotTriviallyCopyable> storage;
	};

	/**
	 * @brief Checks if a coercion from `from` to `to` is possible and returns
	 * a function performing the coercion if it is.
	 */
	CoercionQResult canCoerce(query::Context& ctx, tsh::SymbolType<> from, tsh::SymbolType<> to);

	/**
	 * @brief Checks if a coercion from `from` to the meta type is possible and returns
	 * a function performing the coercion if it is.
	 * @note This is a wrapper around `canCoerce` for the common case of coercing to the meta type.
	 */
	CoercionQResult canCoerceToMeta(query::Context& ctx, tsh::SymbolType<> from);

	/**
	 * @brief Checks whether `expr` can be coerced to `expected_type`. Returns a coerced expression
	 * when the coercion is valid, logs an error via `log_error` when `expr` is not coercible to the
	 * given type.
	 * @note This is a convenience wrapper around `canCoerce` + `coercion.coerce()` for the common
	 * case of coercing expressions with a `Box<code::Expr>` in hand, which is usual when handling
	 * compiler generated code.
	 * @return The coerced expression or an empty optional on error.
	 */
	base::Optional<Box<code::Expr>> coerceFromBox(
		query::Context&                                      ctx,
		Box<code::Expr>                                      expr,
		const tsh::SymbolType<>                              expected_type,
		dia_int::StablePosition                              source_position,
		base::Optional<std::function<void(query::Context&)>> log_error = {}
	);

	/**
	 * @brief A convenience function for typical coercion error logging based on `CoercionQResult`.
	 */
	void logCoercionFailure(
		query::Context&                                      ctx,
		const CoercionQResult&                               coercion_qresult,
		const tsh::SymbolType<>&                             source_symbol_type,
		const tsh::SymbolType<>&                             expected_type,
		dia_int::StablePosition                              source_position,
		base::Optional<std::function<void(query::Context&)>> log_error
	);
}
