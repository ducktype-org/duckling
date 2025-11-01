#pragma once

#include <helios/hout/elements/expr.hpp>
#include <typesystem/higher/symbol_type.hpp>

#include <query_framework/context.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {
	struct InvalidCoercion final {};
	class Coercion;

	/**
	 * @brief Checks if a coercion from `from` to `to` is possible and returns
	 * a function performing the coercion if it is.
	 */
	query::QResult<Coercion, InvalidCoercion> canCoerce(
		query::Context& ctx, tsh::SymbolType<> from, tsh::SymbolType<> to
	);

	/**
	 * @brief This struct represents a function that performs a coercion from one expression to
	 * another. It was added to make sure that the coercion is always valid (by calling
	 * `canCoerce` first), and it checks if the expression being coerced and desired type
	 * is the same as the one validated.
	 */
	class Coercion {
	public:
		/**
		 * Main function that creates a coerced expression from the old one.
		 */
		[[nodiscard]] Box<code::Expr> coerce(Box<code::Expr> from) const;
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

		friend query::QResult<Coercion, InvalidCoercion> canCoerce(
			query::Context& ctx, tsh::SymbolType<> from, tsh::SymbolType<> to
		);

	private:
		Coercion(tsh::SymbolType<> validated_from, tsh::SymbolType<> to):
			  validated_from(validated_from),
			  to(to) {}
	};

}
